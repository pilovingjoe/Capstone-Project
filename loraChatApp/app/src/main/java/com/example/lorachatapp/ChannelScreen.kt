package com.example.lorachatapp

import android.bluetooth.BluetoothDevice
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier

@Composable
fun ChannelScreen(
    activeDevice: BluetoothDevice,
    modifier: Modifier,
    sender: String,
    messageList: MutableList<Message>
) {
    Column(
        modifier.fillMaxSize()
    ) {

    }
}