package dev.roottap.mobile.data.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.bluetooth.le.*
import android.content.Context
import android.os.ParcelUuid
import kotlinx.coroutines.channels.awaitClose
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.callbackFlow

data class DiscoveredDevice(
    val name: String?,
    val address: String,
    val rssi: Int,
)

class BleScanner(private val context: Context) {
    private val bluetoothAdapter: BluetoothAdapter? by lazy {
        val mgr = context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        mgr.adapter
    }

    @SuppressLint("MissingPermission") // call only after permissions granted
    fun scan(
        serviceUuid: ParcelUuid? = null,
        scanMode: Int = ScanSettings.SCAN_MODE_LOW_LATENCY,
    ): Flow<DiscoveredDevice> = callbackFlow {
        val adapter = bluetoothAdapter
        if (adapter == null || !adapter.isEnabled) {
            close(IllegalStateException("Bluetooth is off or unavailable"))
            return@callbackFlow
        }

        val scanner = adapter.bluetoothLeScanner
            ?: run {
                close(IllegalStateException("BLE scanner unavailable"))
                return@callbackFlow
            }

        val filters = buildList {
            if (serviceUuid != null) add(ScanFilter.Builder().setServiceUuid(serviceUuid).build())
        }

        val settings = ScanSettings.Builder()
            .setScanMode(scanMode)
            .build()

        val cb = object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult) {
                trySend(
                    DiscoveredDevice(
                        name = result.device.name ?: result.scanRecord?.deviceName,
                        address = result.device.address,
                        rssi = result.rssi
                    )
                )
            }

            override fun onScanFailed(errorCode: Int) {
                close(RuntimeException("Scan failed: $errorCode"))
            }
        }

        scanner.startScan(filters, settings, cb)

        awaitClose {
            runCatching { scanner.stopScan(cb) }
        }
    }
}
