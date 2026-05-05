package com.example.lorachatapp.viewmodel

import android.Manifest
import android.annotation.SuppressLint
import android.app.Application
import android.bluetooth.BluetoothDevice
import android.content.pm.PackageManager
import androidx.annotation.RequiresPermission
import androidx.core.app.ActivityCompat
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.example.lorachatapp.AppDatabase
import com.example.lorachatapp.Message
import com.tdcolvin.bleclient.ble.BLEScanner
import com.example.lorachatapp.ble.BLEDeviceConnection
import com.tdcolvin.bleclient.ble.PERMISSION_BLUETOOTH_CONNECT
import com.tdcolvin.bleclient.ble.PERMISSION_BLUETOOTH_SCAN
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.*
import kotlinx.coroutines.launch
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import com.example.lorachatapp.DBMessage

// Much of this code is adapted from Rainder Bhandari's "Beginner's Guide to Building a BLE App with Android"
// This counts as "derivative work" under the Apache License, which is included in this repository

@OptIn(ExperimentalCoroutinesApi::class)
class BLEClientViewModel(private val application: Application): AndroidViewModel(application) {
    private val db = AppDatabase.getDatabase(application)
    private val messageDao = db.messageDao()

    val contacts: Flow<List<String>> = messageDao.getUniqueContacts()

    private val bleScanner = BLEScanner(application)
    var activeConnection = MutableStateFlow<BLEDeviceConnection?>(null)

    private val isDeviceConnected = activeConnection.flatMapLatest { it?.isConnected ?: flowOf(false) }
    private val activeDeviceServices = activeConnection.flatMapLatest {
        it?.services ?: flowOf(emptyList())
    }

    private val _uiState = MutableStateFlow(BLEClientUIState())
    val uiState = combine(
        _uiState,
        isDeviceConnected,
        activeDeviceServices,
    ) { state, isDeviceConnected, services ->
        state.copy(
            isDeviceConnected = isDeviceConnected,
            discoveredCharacteristics = services.associate { service -> Pair(service.uuid.toString(), service.characteristics.map { it.uuid.toString() }) },
        )
    }.stateIn(viewModelScope, SharingStarted.WhileSubscribed(5000), BLEClientUIState())

    init {
        seedDatabaseIfEmpty()
        
        viewModelScope.launch {
            activeConnection.flatMapLatest { connection ->
                connection?.readMsg ?: flowOf(null)
            }.collect { rawMessage ->
                rawMessage?.let { parseAndSaveMessage(it) }
            }
        }

        // Automatically discover services and start receiving messages
        var isReceivingStarted = false
        viewModelScope.launch {
            isDeviceConnected.collect { connected ->
                if (connected) {
                    try {
                        activeConnection.value?.discoverServices()
                    } catch (e: SecurityException) {
                        e.printStackTrace()
                    }
                } else {
                    isReceivingStarted = false
                }
            }
        }

        viewModelScope.launch {
            activeDeviceServices.collect { services ->
                if (!isReceivingStarted && services.any { it.uuid == com.example.lorachatapp.ble.UART_SERVICE_UUID }) {
                    try {
                        activeConnection.value?.startReceivingMessages()
                        isReceivingStarted = true
                    } catch (e: SecurityException) {
                        e.printStackTrace()
                    }
                }
            }
        }

        viewModelScope.launch {
            bleScanner.foundDevices.collect { devices ->
                _uiState.update { it.copy(foundDevices = devices) }
            }
        }
        viewModelScope.launch {
            bleScanner.isScanning.collect { isScanning ->
                _uiState.update { it.copy(isScanning = isScanning) }
            }
        }
    }

    @RequiresPermission(PERMISSION_BLUETOOTH_SCAN)
    fun startScanning() {
        bleScanner.startScanning()
    }

    @RequiresPermission(PERMISSION_BLUETOOTH_SCAN)
    fun stopScanning() {
        bleScanner.stopScanning()
    }

    @SuppressLint("MissingPermission")
    @RequiresPermission(allOf = [PERMISSION_BLUETOOTH_CONNECT, PERMISSION_BLUETOOTH_SCAN])
    fun setActiveDevice(device: BluetoothDevice?) {
        activeConnection.value = device?.run { BLEDeviceConnection(application, device) }
        _uiState.update { it.copy(activeDevice = device) }
    }

    @RequiresPermission(PERMISSION_BLUETOOTH_CONNECT)
    fun connectActiveDevice() {
        activeConnection.value?.connect()
    }

    @RequiresPermission(PERMISSION_BLUETOOTH_CONNECT)
    fun disconnectActiveDevice() {
        activeConnection.value?.disconnect()
    }

    @RequiresPermission(PERMISSION_BLUETOOTH_CONNECT)
    fun discoverActiveDeviceServices() {
        activeConnection.value?.discoverServices()
    }

    @RequiresPermission(PERMISSION_BLUETOOTH_CONNECT)
    fun startReceivingMessages() {
        activeConnection.value?.startReceivingMessages()
    }

    @RequiresPermission(PERMISSION_BLUETOOTH_CONNECT)
    fun readMessageFromActiveDevice(): ByteArray? {
        return activeConnection.value?.readMessage()?.value
    }

    @RequiresPermission(PERMISSION_BLUETOOTH_CONNECT)
    fun writeMessageToActiveDevice(message: Message) {
        activeConnection.value?.writeMessage(message)
    }

    override fun onCleared() {
        super.onCleared()

        //when the ViewModel dies, shut down the BLE client with it
        if (bleScanner.isScanning.value) {
            if (ActivityCompat.checkSelfPermission(
                    getApplication(),
                    Manifest.permission.BLUETOOTH_SCAN
                ) == PackageManager.PERMISSION_GRANTED
            ) {
                bleScanner.stopScanning()
            }
        }
    }

    private fun seedDatabaseIfEmpty() {
        viewModelScope.launch(Dispatchers.IO) {
            val existing = messageDao.getAll()
            if (existing.isEmpty()) {
                val time = java.text.SimpleDateFormat("yyyy-MM-dd HH:mm:ss", java.util.Locale.getDefault()).format(java.util.Date())
                messageDao.insertAll(
                    DBMessage(content = "Hello from Alice!", sender = "Alice", receiver = "Me", dateTime = time, isMe = false),
                    DBMessage(content = "Hey Bob, you there?", sender = "Me", receiver = "Bob", dateTime = time, isMe = true),
                    DBMessage(content = "System Update: Welcome to LoraChat", sender = "System", receiver = "Me", dateTime = time, isMe = false)
                )
            }
        }
    }

    private fun parseAndSaveMessage(raw: String) {
        viewModelScope.launch(Dispatchers.IO) {
            try {
                // Format: receiver;sender;dateTime;content
                val parts = raw.split(";")
                if (parts.size >= 4) {
                    val dbMsg = DBMessage(
                        receiver = parts[0],
                        sender = parts[1],
                        dateTime = parts[2],
                        content = parts[3],
                        isMe = parts[1] == "Me" // or however you determine if it's from you
                    )
                    messageDao.insertAll(dbMsg)
                }
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }
    }
}

data class BLEClientUIState(
    val isScanning: Boolean = false,
    val foundDevices: List<BluetoothDevice> = emptyList(),
    val activeDevice: BluetoothDevice? = null,
    val isDeviceConnected: Boolean = false,
    val discoveredCharacteristics: Map<String, List<String>> = emptyMap(),
)