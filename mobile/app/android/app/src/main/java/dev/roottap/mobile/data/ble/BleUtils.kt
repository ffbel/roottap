package dev.roottap.mobile.data.ble

import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice

fun bluetoothDeviceFromAddress(address: String): BluetoothDevice? {
    val adapter = BluetoothAdapter.getDefaultAdapter() ?: return null
    return try {
        adapter.getRemoteDevice(address)
    } catch (e: IllegalArgumentException) {
        null
    }
}
