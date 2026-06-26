package com.example.deskbuddy

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import androidx.core.content.ContextCompat

class BootReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context?, intent: Intent?) {
        if (intent?.action == Intent.ACTION_BOOT_COMPLETED && context != null) {
            val prefs = context.getSharedPreferences("deskbuddy_prefs", Context.MODE_PRIVATE)
            val pairedMac = prefs.getString("paired_mac", null)
            if (pairedMac != null) {
                val serviceIntent = Intent(context, DeskBuddyService::class.java)
                ContextCompat.startForegroundService(context, serviceIntent)
            }
        }
    }
}
