package com.example.lorachatapp
import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.annotation.RequiresPermission
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Person
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.example.lorachatapp.ble.UART_SERVICE_UUID
import com.example.lorachatapp.ui.theme.LoraChatAppTheme
import com.example.lorachatapp.viewmodel.BLEClientUIState
import com.example.lorachatapp.viewmodel.BLEClientViewModel
import kotlin.collections.contains
import androidx.compose.runtime.collectAsState
import java.nio.DoubleBuffer

@SuppressLint("MissingPermission")
class MainActivity : ComponentActivity() {
    @OptIn(ExperimentalMaterial3Api::class)
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            LoraChatAppTheme {
                Scaffold(
                    topBar = {
                        var isScanning by remember {
                            mutableStateOf(false)
                        }
                        var havePerms by remember {
                            mutableStateOf(haveAllPermissions(applicationContext))
                        }
                        TopAppBar(
                            title = { Text("Lora Chat Channels") },
                            actions = {
                                if (!(havePerms))
                                    GrantPermissionsButton {
                                        havePerms = true
                                    }
                            }

                        )
                    }
                ) { innerPadding ->

                    val viewModel: BLEClientViewModel by viewModels()
                    val contacts by viewModel.contacts.collectAsState(initial = emptyList())
                    Surface(
                        modifier = Modifier.fillMaxSize(),
                        color = MaterialTheme.colorScheme.background
                    ) {
                        MyApplication.setData(viewModel.activeConnection.collectAsState().value)
                        MainScreen(
                            senders = contacts,
                            onSenderClick = { sender, viewModel ->
                                val intent = Intent(this, ChannelActivity::class.java).apply {
                                    putExtra("SENDER_NAME", sender)
                                }
                                startActivity(intent)
                            },
                            modifier = Modifier.padding(innerPadding),
                            viewModel
                        )
                    }
                }
            }
        }
    }
}

@RequiresPermission(allOf = [Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT])
@Composable
fun MainScreen(
    senders: List<String>,
    onSenderClick: (String, BLEClientViewModel) -> Unit,
    modifier: Modifier = Modifier,
    viewModel: BLEClientViewModel
) {
    val uiState by viewModel.uiState.collectAsStateWithLifecycle()
    val context = LocalContext.current
    val connected = false
    if (uiState.activeDevice == null) {
        ScanningScreen(
            isScanning = uiState.isScanning,
            foundDevices = uiState.foundDevices,
            startScanning = viewModel::startScanning,
            stopScanning = viewModel::stopScanning,
            selectDevice = { device ->
                viewModel.stopScanning()
                viewModel.setActiveDevice(device)
            },
            modifier = modifier
        )
//    } else if (!(uiState.isDeviceConnected)) {
    } else if (!uiState.discoveredCharacteristics.contains(UART_SERVICE_UUID.toString())) {
        DeviceScreen(
            isDeviceConnected = uiState.isDeviceConnected,
            discoveredCharacteristics = uiState.discoveredCharacteristics,
            connect = viewModel::connectActiveDevice,
            discoverServices = viewModel::discoverActiveDeviceServices,
            modifier = modifier
        )
    } else {
        if (senders.isEmpty()) {
            Box(modifier = modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                Text(text = "No channels available.")
            }
        } else {
            LazyColumn(modifier = modifier.fillMaxSize()) {
                items(senders) { sender ->
                    ChannelItem(sender = sender, onClick = { onSenderClick(sender, viewModel) })
                    HorizontalDivider()
                }
            }
        }
    }
}

@Composable
fun ChannelItem(sender: String, onClick: () -> Unit) {
    ListItem(
        headlineContent = { Text(sender) },
        supportingContent = { Text("Click to open chat") },
        leadingContent = {
            Icon(
                Icons.Default.Person,
                contentDescription = null
            )
        },
        modifier = Modifier
            .fillMaxWidth()
            .clickable { onClick() }
            .padding(8.dp)
    )
}

val ALL_BLE_PERMISSIONS = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
    arrayOf(
        Manifest.permission.BLUETOOTH_CONNECT,
        Manifest.permission.BLUETOOTH_SCAN
    )
} else {
    arrayOf(
        Manifest.permission.BLUETOOTH_ADMIN,
        Manifest.permission.BLUETOOTH,
        Manifest.permission.ACCESS_FINE_LOCATION
    )
}

@Composable
fun GrantPermissionsButton(onPermissionGranted: () -> Unit) {
    val launcher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.RequestMultiplePermissions()
    ) { granted ->
        if (granted.values.all { it }) {
            // User has granted all permissions
            onPermissionGranted()
        }
    }

    // User presses this button to request permissions
    Button(onClick = { launcher.launch(ALL_BLE_PERMISSIONS) }) {
        Text("Grant Permission")
    }
}
fun haveAllPermissions(context: Context) =
    ALL_BLE_PERMISSIONS
        .all { context.checkSelfPermission(it) == PackageManager.PERMISSION_GRANTED }

