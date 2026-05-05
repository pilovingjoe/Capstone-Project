package com.example.lorachatapp

import androidx.compose.foundation.gestures.Orientation
import androidx.compose.foundation.gestures.scrollable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.material3.Button
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.example.lorachatapp.ble.UART_SERVICE_UUID

@Composable
fun DeviceScreen(
    isDeviceConnected: Boolean,
    discoveredCharacteristics: Map<String, List<String>>,
    connect: () -> Unit,
    discoverServices: () -> Unit,
    modifier: Modifier
) {
    val foundTargetService = discoveredCharacteristics.contains(UART_SERVICE_UUID.toString())

    Column(
        modifier.scrollable(rememberScrollState(), Orientation.Vertical)
    ) {
        Button(onClick = connect) {
            Text("1. Connect")
        }
        Text("Device connected: $isDeviceConnected")
        Button(onClick = discoverServices, enabled = isDeviceConnected) {
            Text("2. Discover Services")
        }
        if (foundTargetService) {
            Text("✓ UART Service found, receiving messages...", color = androidx.compose.ui.graphics.Color.Green)
        }
        LazyColumn(modifier = Modifier.weight(1f)) {
            items(discoveredCharacteristics.keys.sorted()) { serviceUuid ->
                Text(text = serviceUuid, fontWeight = FontWeight.Black)
                Column(modifier = Modifier.padding(start = 10.dp)) {
                    discoveredCharacteristics[serviceUuid]?.forEach {
                        Text(it)
                    }
                }
            }
        }
    }
}
