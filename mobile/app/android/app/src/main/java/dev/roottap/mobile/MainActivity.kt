package dev.roottap.mobile

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.core.view.WindowCompat
import dev.roottap.mobile.core.permissions.bleRuntimePermissions
import dev.roottap.mobile.data.ble.BleScanner
import dev.roottap.mobile.data.ble.DiscoveredDevice
import dev.roottap.mobile.data.ble.GattClient
import dev.roottap.mobile.data.ble.bluetoothDeviceFromAddress
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.CancellationException
import java.util.UUID

class MainActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        WindowCompat.setDecorFitsSystemWindows(window, false)

        setContent {
            MaterialTheme {
                ScanScreen()
            }
        }
    }
}

@Composable
private fun ScanScreen() {
    val context = androidx.compose.ui.platform.LocalContext.current
    val scanner = remember { BleScanner(context) }

    val gattClient = remember {
        GattClient(
            context = context,
            serviceUuid = UUID.fromString("d173119b-a021-2f9e-6a4b-778c6f2e1c5a"),
            notifyCharUuid = UUID.fromString("d373119b-a021-2f9e-6a4b-778c6f2e1c5a"),
            writeCharUuid = UUID.fromString("d273119b-a021-2f9e-6a4b-778c6f2e1c5a"),
        )
    }


    var hasPerms by remember { mutableStateOf(false) }
    val devices = remember { mutableStateMapOf<String, DiscoveredDevice>() }
    var scanning by remember { mutableStateOf(false) }
    var error by remember { mutableStateOf<String?>(null) }
    var connected by remember { mutableStateOf(false) }

    val TARGET_NAME = "roottap-up"

    val launcher = rememberLauncherForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { result: Map<String, Boolean> ->
        hasPerms = result.values.all { it }
    }

    LaunchedEffect(Unit) {
        launcher.launch(bleRuntimePermissions())
    }

    LaunchedEffect(scanning, hasPerms) {
        if (!scanning || !hasPerms || connected) return@LaunchedEffect
        error = null
        devices.clear()

        runCatching {
            scanner.scan(serviceUuid = null).collectLatest { d ->
                devices[d.address] = d

                if (!connected && d.name?.equals(TARGET_NAME) == true) {
                    connected = true    // prevent repeated connects
                    scanning = false    // stop scan UI state (will cancel scan coroutine)
                    val device = bluetoothDeviceFromAddress(d.address)
                    if (device != null) {
                        gattClient.connect(device)
                    } else {
                        error = "Failed to resolve BluetoothDevice"
                        connected = false
                    }
                }
            }
        }.onFailure {
            if (it is CancellationException) return@onFailure   // <-- IMPORTANT: ignore normal cancellation
            error = it.message
            scanning = false
        }
    }

    Column(
        Modifier
            .fillMaxSize()
            .systemBarsPadding()
            .padding(16.dp)
    ) {
        Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            Button(
                onClick = {
                    scanning = !scanning
                    if (!scanning) error = null
                          },
                enabled = hasPerms
            ) { Text(if (scanning) "Stop scan" else "Start scan") }

            if (!hasPerms) {
                OutlinedButton(onClick = { launcher.launch(bleRuntimePermissions()) }) {
                    Text("Grant permissions")
                }
            }
        }

        if (error != null) {
            Spacer(Modifier.height(12.dp))
            Text("Error: $error", color = MaterialTheme.colorScheme.error)
        }

        Spacer(Modifier.height(12.dp))

        LazyColumn(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            items(devices.values.sortedByDescending { it.rssi }) { d ->
                Card(Modifier.fillMaxWidth()) {
                    Column(Modifier.padding(12.dp)) {
                        Text(d.name ?: "(no name)")
                        Text(d.address, style = MaterialTheme.typography.bodySmall)
                        Text("RSSI: ${d.rssi}", style = MaterialTheme.typography.bodySmall)
                    }
                }
            }
        }
    }
}
