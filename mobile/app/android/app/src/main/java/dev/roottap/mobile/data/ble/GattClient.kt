package dev.roottap.mobile.data.ble

import android.annotation.SuppressLint
import android.bluetooth.*
import android.content.Context
import android.util.Log
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.util.UUID

enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED
}

class GattClient(
    private val context: Context,
    private val serviceUuid: UUID,
    private val notifyCharUuid: UUID,
    private val writeCharUuid: UUID,
) {
    private var device: BluetoothDevice? = null
    private var gatt: BluetoothGatt? = null
    private var notifyChar: BluetoothGattCharacteristic? = null
    private var writeChar: BluetoothGattCharacteristic? = null

    private val _connectionState = MutableStateFlow(ConnectionState.DISCONNECTED)
    val connectionState = _connectionState.asStateFlow()

    private val cccdUuid: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    private val tag = "RootTapGatt"
    private var notifReady = false

    @SuppressLint("MissingPermission")
    fun connect(device: BluetoothDevice) {
        if (_connectionState.value != ConnectionState.DISCONNECTED) {
            Log.w(tag, "Connect called while not in DISCONNECTED state.")
            return
        }
        this.device = device
        Log.d(tag, "Connecting to ${device.address} ...")
        _connectionState.value = ConnectionState.CONNECTING
        this.gatt = device.connectGatt(context, false, cb, BluetoothDevice.TRANSPORT_LE)
    }

    @SuppressLint("MissingPermission")
    fun disconnect() {
        Log.d(tag, "User-initiated disconnect.")
        // 1. Prevent auto-reconnect by clearing the target device.
        device = null
        // 2. Trigger GATT disconnection if a GATT object exists.
        //    The callback will handle cleanup and final state change.
        gatt?.disconnect()
        // 3. Update UI immediately for responsiveness.
        _connectionState.value = ConnectionState.DISCONNECTED
    }

    private val cb: BluetoothGattCallback = object : BluetoothGattCallback() {

        @SuppressLint("MissingPermission")
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            val gattDeviceAddress = gatt.device.address

            // An event from a device we don't want to be connected to anymore.
            if (device == null || gattDeviceAddress != device!!.address) {
                // This can happen if user calls disconnect() then the callback arrives.
                Log.d(tag, "Ignoring stale callback from $gattDeviceAddress.")
                gatt.close()
                return
            }

            if (newState == BluetoothProfile.STATE_CONNECTED) {
                Log.d(tag, "Connected to $gattDeviceAddress, discovering services...")
                _connectionState.value = ConnectionState.CONNECTED
                gatt.discoverServices()
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                Log.d(tag, "Disconnected from $gattDeviceAddress")
                // Always close the gatt object that has disconnected to release resources.
                gatt.close()
                this@GattClient.gatt = null

                // Decide what to do next based on whether user wants to be connected.
                device?.let {
                    // This was an unexpected disconnect.
                    Log.d(tag, "Reconnecting to ${it.address} ...")
                    _connectionState.value = ConnectionState.CONNECTING
                    this@GattClient.gatt = it.connectGatt(context, false, this, BluetoothDevice.TRANSPORT_LE)
                } ?: run {
                    // This was an expected disconnect (user called disconnect()).
                    _connectionState.value = ConnectionState.DISCONNECTED
                }
            }
        }

        @SuppressLint("MissingPermission")
        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                Log.e(tag, "Service discovery failed with status $status")
                gatt.disconnect()
                return
            }

            Log.d(tag, "services discovered")
            val svc = gatt.getService(serviceUuid)
            if (svc == null) {
                Log.e(tag, "Service not found: $serviceUuid")
                gatt.disconnect()
                return
            }

            notifyChar = svc.getCharacteristic(notifyCharUuid)
            writeChar = svc.getCharacteristic(writeCharUuid)

            if (notifyChar == null || writeChar == null) {
                Log.e(tag, "Characteristics not found notify=$notifyChar write=$writeChar")
                gatt.disconnect()
                return
            }

            enableNotifications(gatt, notifyChar!!)
        }

        // For Android 13+ there's also onCharacteristicChanged with value param; this one still works widely.
        @Deprecated("Deprecated in API 33 but still called on many devices")
        override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            if (!notifReady) return
            val value = characteristic.value ?: return
            Log.d(tag, "notify ${characteristic.uuid} value=${value.joinToString { "%02X".format(it) }}")
            if (characteristic.uuid == notifyCharUuid && value.isNotEmpty() && value[0].toInt() == 0x01) {
                // GPIO pressed -> respond with 0x01
                write(byteArrayOf(0x01))
            }
        }

        @SuppressLint("MissingPermission")
        override fun onDescriptorWrite(gatt: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) {
            Log.d(tag, "descriptor write ${descriptor.uuid} status=$status")
            notifReady = (status == BluetoothGatt.GATT_SUCCESS)
        }

        @SuppressLint("MissingPermission")
        override fun onCharacteristicWrite(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
            Log.d(tag, "write ${characteristic.uuid} status=$status")
        }
    }

    @SuppressLint("MissingPermission")
    private fun enableNotifications(gatt: BluetoothGatt, c: BluetoothGattCharacteristic) {
        val ok = gatt.setCharacteristicNotification(c, true)
        Log.d(tag, "setCharacteristicNotification=$ok")

        val cccd = c.getDescriptor(cccdUuid)
        if (cccd == null) {
            Log.e(tag, "CCCD not found on notify characteristic")
            return
        }
        cccd.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
        gatt.writeDescriptor(cccd)
    }

    @SuppressLint("MissingPermission")
    private fun write(payload: ByteArray) {
        val g = gatt ?: return
        val c = writeChar ?: return

        c.value = payload

        // Prefer WRITE_NO_RESPONSE if your char supports it (faster).
        val supportsNoResp = (c.properties and BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE) != 0
        c.writeType = if (supportsNoResp)
            BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
        else
            BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT

        val ok = g.writeCharacteristic(c)
        Log.d(tag, "writeCharacteristic ok=$ok payload=${payload.joinToString { "%02X".format(it) }}")
    }
}
