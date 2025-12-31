package dev.roottap.mobile.data.ble

import android.annotation.SuppressLint
import android.bluetooth.*
import android.content.Context
import android.util.Log
import java.util.UUID

class GattClient(
    private val context: Context,
    private val serviceUuid: UUID,
    private val notifyCharUuid: UUID,
    private val writeCharUuid: UUID,
) {
    private var gatt: BluetoothGatt? = null
    private var notifyChar: BluetoothGattCharacteristic? = null
    private var writeChar: BluetoothGattCharacteristic? = null

    private val cccdUuid: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    private val tag = "RootTapGatt"
    private var notifReady = false

    @SuppressLint("MissingPermission")
    fun connect(device: BluetoothDevice) {
        Log.d(tag, "Connecting to ${device.address} ...")
        gatt = device.connectGatt(context, false, cb, BluetoothDevice.TRANSPORT_LE)
    }

    @SuppressLint("MissingPermission")
    fun disconnect() {
        gatt?.disconnect()
        gatt?.close()
        gatt = null
    }

    private val cb = object : BluetoothGattCallback() {

        @SuppressLint("MissingPermission")
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            Log.d(tag, "conn state status=$status newState=$newState")
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                Log.d(tag, "Connected, discovering services...")
                gatt.discoverServices()
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                Log.d(tag, "Disconnected")
                disconnect()
            }
        }

        @SuppressLint("MissingPermission")
        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            Log.d(tag, "services discovered status=$status")
            val svc = gatt.getService(serviceUuid)
            if (svc == null) {
                Log.e(tag, "Service not found: $serviceUuid")
                return
            }

            notifyChar = svc.getCharacteristic(notifyCharUuid)
            writeChar = svc.getCharacteristic(writeCharUuid)

            if (notifyChar == null || writeChar == null) {
                Log.e(tag, "Characteristics not found notify=$notifyChar write=$writeChar")
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
        c.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
//        val supportsNoResp = (c.properties and BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE) != 0
//        c.writeType = if (supportsNoResp)
//            BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
//        else
//            BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT

        val ok = g.writeCharacteristic(c)
        Log.d(tag, "writeCharacteristic ok=$ok payload=${payload.joinToString { "%02X".format(it) }}")
    }
}
