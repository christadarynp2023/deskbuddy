package com.example.deskbuddy

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.BatteryManager
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.util.Log
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat
import java.util.TimeZone

class DeskBuddyService : Service() {

    private val tag = "DeskBuddyService"
    private var bleManager: BleManager? = null
    private var callReceiver: CallReceiver? = null
    private val handler = Handler(Looper.getMainLooper())
    private var lastPairedMac: String? = null

    companion object {
        var instance: DeskBuddyService? = null
    }

    override fun onCreate() {
        super.onCreate()
        instance = this
        startForegroundNotification()

        val prefs = getSharedPreferences("deskbuddy_prefs", Context.MODE_PRIVATE)
        lastPairedMac = prefs.getString("paired_mac", null)

        val adapter = (getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager).adapter
        if (adapter != null && lastPairedMac != null) {
            bleManager = BleManager(this, adapter, lastPairedMac!!)
            bleManager?.connect()
        }

        callReceiver = CallReceiver()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            registerReceiver(callReceiver, IntentFilter("android.intent.action.PHONE_STATE"), Context.RECEIVER_EXPORTED)
        } else {
            registerReceiver(callReceiver, IntentFilter("android.intent.action.PHONE_STATE"))
        }

        // Start periodic sync (Time and Battery level) every 5 minutes
        handler.post(syncRunnable)
    }

    override fun onDestroy() {
        super.onDestroy()
        handler.removeCallbacks(syncRunnable)
        bleManager?.disconnect()
        try {
            unregisterReceiver(callReceiver)
        } catch (e: Exception) {}
        instance = null
    }

    override fun onBind(intent: Intent?): IBinder? = null

    private fun startForegroundNotification() {
        val channelId = "deskbuddy_link"
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(channelId, "Desk Buddy Link", NotificationManager.IMPORTANCE_LOW)
            val manager = getSystemService(NotificationManager::class.java)
            manager.createNotificationChannel(channel)
        }
        val notification = NotificationCompat.Builder(this, channelId)
            .setContentTitle("Desk Buddy Active Link")
            .setContentText("Listening for phone calls and connected to Desk Buddy.")
            .setSmallIcon(android.R.drawable.stat_sys_phone_call)
            .build()
        startForeground(2, notification)
    }

    fun isBleConnected(): Boolean = bleManager?.isConnected() == true

    fun sendCallAlert(caller: String) {
        bleManager?.writeCallState("R:$caller")
    }

    fun sendCallEnd() {
        bleManager?.writeCallState("I")
    }

    fun syncTime() {
        val epochSeconds = System.currentTimeMillis() / 1000
        val timezoneId = TimeZone.getDefault().id
        bleManager?.writeSyncInfo("T:$epochSeconds:$timezoneId")
    }

    fun findDeskBuddy() {
        bleManager?.writeSyncInfo("F")
    }

    fun performOtaUpdate(bytes: ByteArray, progressCallback: (Int) -> Unit) {
        bleManager?.let { manager ->
            val otaManager = OtaManager(manager, bytes, progressCallback)
            otaManager.start()
        }
    }

    private val syncRunnable = object : Runnable {
        override fun run() {
            if (isBleConnected()) {
                syncTime()
                
                // Get Battery Percentage
                val bm = getSystemService(Context.BATTERY_SERVICE) as BatteryManager
                val batteryPct = bm.getIntProperty(BatteryManager.BATTERY_PROPERTY_CAPACITY)
                bleManager?.writeSyncInfo("B:$batteryPct")
            }
            handler.postDelayed(this, 300000) // Repeat every 5 minutes
        }
    }
}
