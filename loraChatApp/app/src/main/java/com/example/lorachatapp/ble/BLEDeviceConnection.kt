package com.example.lorachatapp.ble

import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothGattService
import android.content.Context
import android.util.Log
import androidx.annotation.RequiresPermission
import androidx.compose.ui.platform.ClipEntry
import com.example.lorachatapp.Message
import com.tdcolvin.bleclient.ble.PERMISSION_BLUETOOTH_CONNECT
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.update
import java.util.UUID

val UART_SERVICE_UUID: UUID = UUID.fromString("6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
val WRITE_CHARACTERISTIC_UUID: UUID = UUID.fromString("6E400002-B5A3-F393-E0A9-E50E24DCCA9E")
val READ_CHARACTERISTIC_UUID: UUID = UUID.fromString("8c380002-10bd-4fdb-ba21-1922d6cf860d")

@Suppress("DEPRECATION")
class BLEDeviceConnection @RequiresPermission("PERMISSION_BLUETOOTH_CONNECT") constructor(
    private val context: Context,
    private val bluetoothDevice: BluetoothDevice
) {
    val isConnected = MutableStateFlow(false)
    val readMsg = MutableStateFlow<String?>(null)
    val successfulNameWrites = MutableStateFlow(0)
    val services = MutableStateFlow<List<BluetoothGattService>>(emptyList())

    private val callback = object: BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            super.onConnectionStateChange(gatt, status, newState)
            val connected = newState == BluetoothGatt.STATE_CONNECTED
            if (connected) {
                //read the list of services
                services.value = gatt.services
            }
            isConnected.value = connected
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            super.onServicesDiscovered(gatt, status)
            services.value = gatt.services
        }

        @Deprecated("Deprecated in Java")
        override fun onCharacteristicRead(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            status: Int
        ) {
            super.onCharacteristicRead(gatt, characteristic, status)
            if (characteristic.uuid == READ_CHARACTERISTIC_UUID) {
                readMsg.value = String(characteristic.value)
            }
        }

        override fun onCharacteristicWrite(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            status: Int
        ) {
            super.onCharacteristicWrite(gatt, characteristic, status)
            if (characteristic.uuid == WRITE_CHARACTERISTIC_UUID) {
                successfulNameWrites.update { it + 1 }
            }
        }

        @Deprecated("Deprecated in Java")
        override fun onCharacteristicChanged(
            gatt: BluetoothGatt?,
            characteristic: BluetoothGattCharacteristic?
        ) {
            super.onCharacteristicChanged(gatt, characteristic)
            characteristic?.let {
                val value = String(it.value)
                Log.v("bluetooth", "Characteristic changed: $value")
                readMsg.value = value
            }
        }
    }

    private var gatt: BluetoothGatt? = null

    @RequiresPermission(PERMISSION_BLUETOOTH_CONNECT)
    fun disconnect() {
        gatt?.disconnect()
        gatt?.close()
        gatt = null
    }

    @RequiresPermission(PERMISSION_BLUETOOTH_CONNECT)
    fun connect() {
        gatt = bluetoothDevice.connectGatt(context, false, callback)
    }

    @RequiresPermission(PERMISSION_BLUETOOTH_CONNECT)
    fun discoverServices() {
        gatt?.discoverServices()
    }

    @RequiresPermission(PERMISSION_BLUETOOTH_CONNECT)
    fun startReceivingMessages(){
        val service = gatt?.getService(UART_SERVICE_UUID)
        val characteristic = service?.getCharacteristic(READ_CHARACTERISTIC_UUID)
        if(characteristic!=null){
            gatt?.setCharacteristicNotification(characteristic,true)
            val CLIENT_CONFIG_DESCRIPTOR = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
            val desc = characteristic.getDescriptor(CLIENT_CONFIG_DESCRIPTOR)
            desc?.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
            gatt?.writeDescriptor(desc)
        }
    }

    @RequiresPermission(PERMISSION_BLUETOOTH_CONNECT)
    fun readMessage(): BluetoothGattCharacteristic? {
        val service = gatt?.getService(UART_SERVICE_UUID)
        val characteristic = service?.getCharacteristic(READ_CHARACTERISTIC_UUID)
        if (characteristic != null) {
            val success = gatt?.readCharacteristic(characteristic)
            Log.v("bluetooth", "Read status: $success")
        }
        return characteristic
    }


    @RequiresPermission(PERMISSION_BLUETOOTH_CONNECT)
    fun writeMessage(message: Message) {
        val service = gatt?.getService(UART_SERVICE_UUID)
        val characteristic = service?.getCharacteristic(READ_CHARACTERISTIC_UUID)
        if (characteristic != null) {
            characteristic.value = message.content.toByteArray()
            val success = gatt?.writeCharacteristic(characteristic)
            Log.v("bluetooth", "Write status: $success")
        }
    }
}