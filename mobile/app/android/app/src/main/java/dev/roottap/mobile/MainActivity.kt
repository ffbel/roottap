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
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.withStyle
import androidx.compose.ui.unit.dp
import androidx.core.view.WindowCompat
import dev.roottap.mobile.core.permissions.bleRuntimePermissions
import dev.roottap.mobile.data.ble.BleScanner
import dev.roottap.mobile.data.ble.DiscoveredDevice
import dev.roottap.mobile.data.ble.GattClient
import dev.roottap.mobile.data.ble.bluetoothDeviceFromAddress
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch
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

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun ScanScreen() {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
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
                if (d.name?.equals(TARGET_NAME) == true) {
                    devices[d.address] = d
                }
            }
        }.onFailure {
            if (it is CancellationException) return@onFailure   // <-- IMPORTANT: ignore normal cancellation
            error = it.message
            scanning = false
        }
    }

    fun connect(device: DiscoveredDevice) {
        scope.launch {
            val btDevice = bluetoothDeviceFromAddress(device.address)
            if (btDevice != null) {
                gattClient.connect(btDevice)
                connected = true
                scanning = false
            } else {
                error = "Failed to resolve BluetoothDevice"
            }
        }
    }

    fun disconnect() {
        gattClient.disconnect()
        connected = false
        devices.clear()
    }

    Column(
        Modifier
            .fillMaxSize()
            .systemBarsPadding()
            .padding(16.dp)
    ) {
        Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            if (!connected) {
                Button(
                    onClick = {
                        scanning = !scanning
                        if (!scanning) {
                            error = null
                            devices.clear()
                        }
                    },
                    enabled = hasPerms
                ) { Text(if (scanning) "Stop scan" else "Start scan") }
            } else {
                Button(onClick = { disconnect() }) {
                    Text("Disconnect")
                }
            }

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

        if (connected) {
            Text(
                buildAnnotatedString {
                    append("Connected to ")
                    withStyle(style = SpanStyle(fontWeight = FontWeight.Bold)) {
                        append(TARGET_NAME)
                    }
                }
            )
        } else {
            LazyColumn(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                items(devices.values.sortedByDescending { it.rssi }) { d ->
                    Card(
                        onClick = { connect(d) },
                        modifier = Modifier.fillMaxWidth()
                    ) {
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
}
