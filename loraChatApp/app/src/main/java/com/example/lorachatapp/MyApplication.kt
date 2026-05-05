package com.example.lorachatapp

import android.annotation.SuppressLint
import android.app.Application
import androidx.room.Room
import com.example.lorachatapp.ble.BLEDeviceConnection
import com.example.lorachatapp.viewmodel.BLEClientViewModel

// This is a dummy object to pass the BLEDevice from view model to the channel activity
object MyApplication{
    @SuppressLint("StaticFieldLeak")
    var BLEDevice:BLEDeviceConnection? = null
    fun getData(): BLEDeviceConnection?{
        return BLEDevice
    }
    fun setData(view: BLEDeviceConnection?){
        BLEDevice=view
    }

}