package com.example.deskbuddy

import android.bluetooth.*
import android.content.Context
import android.util.Log
import java.util.UUID

class BleManager(
    private val context: Context,
    private val adapter: BluetoothAdapter,
    private val macAddress: String
) {
    private val tag = "BleManager"
    private var bluetoothGatt: BluetoothGatt? = null
    private var isBleConnected = false

    private val serviceUuid = UUID.fromString("4fafc201-1fb5-459e-8fcc-c5c9c331914b")
    private val charCallUuid = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26a8")
    private val charSyncUuid = UUID.fromString("a1234567-1fb5-459e-8fcc-c5c9c331914b")
    private val charOtaUuid = UUID.fromString("b1234567-1fb5-459e-8fcc-c5c9c331914b")

    fun connect() {
        val device = adapter.getRemoteDevice(macAddress)
        try {
            bluetoothGatt = device.connectGatt(context, true, gattCallback, BluetoothDevice.TRANSPORT_LE)
        } catch (e: SecurityException) {
            Log.e(tag, "Connect failed due to permissions exception", e)
        }
    }

    fun disconnect() {
        try {
            bluetoothGatt?.disconnect()
            bluetoothGatt?.close()
            bluetoothGatt = null
            isBleConnected = false
        } catch (e: SecurityException) {}
    }

    fun isConnected(): Boolean = isBleConnected

    fun writeCallState(state: String) {
        writeCharacteristic(charCallUuid, state.toByteArray(Charsets.UTF_8))
    }

    fun writeSyncInfo(info: String) {
        writeCharacteristic(charSyncUuid, info.toByteArray(Charsets.UTF_8))
    }

    fun writeOtaChunk(data: ByteArray) {
        writeCharacteristic(charOtaUuid, data)
    }

    private fun writeCharacteristic(charUuid: UUID, data: ByteArray) {
        val gatt = bluetoothGatt ?: return
        val service = gatt.getService(serviceUuid) ?: return
        val char = service.getCharacteristic(charUuid) ?: return
        char.value = data
        try {
            gatt.writeCharacteristic(char)
        } catch (e: SecurityException) {}
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt?, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                isBleConnected = true
                Log.d(tag, "BLE Connected. Discovering Services...")
                try {
                    gatt?.discoverServices()
                } catch (e: SecurityException) {}
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                isBleConnected = false
                Log.d(tag, "BLE Disconnected. Reconnection scheduled by client...")
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt?, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                Log.d(tag, "GATT Services resolved successfully.")
            }
        }
    }
}
