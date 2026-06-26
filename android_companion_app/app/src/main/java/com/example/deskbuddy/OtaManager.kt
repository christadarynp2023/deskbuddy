package com.example.deskbuddy

import android.os.Handler
import android.os.Looper
import android.util.Log

class OtaManager(
    private val bleManager: BleManager,
    private val firmwareBytes: ByteArray,
    private val progressCallback: (Int) -> Unit
) {
    private val tag = "OtaManager"
    private val handler = Handler(Looper.getMainLooper())

    fun start() {
        Thread {
            try {
                // Send Start header
                val size = firmwareBytes.size
                bleManager.writeOtaChunk("S:$size".toByteArray(Charsets.UTF_8))
                Thread.sleep(500) // Allow ESP32 partition manager to start Update

                val chunkSize = 200 // ESP32 BLE default safe MTU size
                var offset = 0
                var lastProgress = 0

                while (offset < size) {
                    val length = minOf(chunkSize, size - offset)
                    val chunk = ByteArray(length + 2)
                    chunk[0] = 'D'.toByte()
                    chunk[1] = ':'.toByte()
                    System.arraycopy(firmwareBytes, offset, chunk, 2, length)

                    bleManager.writeOtaChunk(chunk)
                    offset += length

                    val progress = ((offset.toFloat() / size.toFloat()) * 100).toInt()
                    if (progress > lastProgress) {
                        lastProgress = progress
                        handler.post { progressCallback(progress) }
                    }
                    Thread.sleep(30) // Prevent packet congestion
                }

                // Send End finalize packet
                bleManager.writeOtaChunk("E".toByteArray(Charsets.UTF_8))
                Log.d(tag, "OTA Firmware update packet transmission completed.")
            } catch (e: Exception) {
                Log.e(tag, "Error during BLE OTA write", e)
            }
        }.start()
    }
}
