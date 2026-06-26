package com.example.deskbuddy

import android.Manifest
import android.app.Activity
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.widget.Button
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import com.google.android.material.button.MaterialButton
import java.io.InputStream

class MainActivity : AppCompatActivity() {

    private val requiredPermissions = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        arrayOf(
            Manifest.permission.BLUETOOTH_SCAN,
            Manifest.permission.BLUETOOTH_CONNECT,
            Manifest.permission.READ_PHONE_STATE,
            Manifest.permission.READ_CALL_LOG,
            Manifest.permission.READ_CONTACTS
        )
    } else {
        arrayOf(
            Manifest.permission.ACCESS_FINE_LOCATION,
            Manifest.permission.READ_PHONE_STATE,
            Manifest.permission.READ_CALL_LOG,
            Manifest.permission.READ_CONTACTS
        )
    }

    private lateinit var txtMac: TextView
    private lateinit var txtStatus: TextView
    private lateinit var btnOta: MaterialButton

    private val selectFileLauncher = registerForActivityResult(ActivityResultContracts.StartActivityForResult()) { result ->
        if (result.resultCode == Activity.RESULT_OK) {
            val uri: Uri? = result.data?.data
            uri?.let { startOtaUpload(it) }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        txtMac = findViewById(R.id.txtMac)
        txtStatus = findViewById(R.id.txtStatus)
        btnOta = findViewById(R.id.btnOta)
        val btnSync: Button = findViewById(R.id.btnSync)
        val btnFind: Button = findViewById(R.id.btnFind)

        // Parse Deep Link Pairing
        intent?.data?.let { uri ->
            val mac = uri.getQueryParameter("mac")
            if (!mac.isNullOrEmpty()) {
                val prefs = getSharedPreferences("deskbuddy_prefs", Context.MODE_PRIVATE)
                prefs.edit().putString("paired_mac", mac.uppercase()).apply()
                Toast.makeText(this, "Paired Device MAC: $mac", Toast.LENGTH_LONG).show()
            }
        }

        checkAndRequestPermissions()

        val prefs = getSharedPreferences("deskbuddy_prefs", Context.MODE_PRIVATE)
        val pairedMac = prefs.getString("paired_mac", "None")
        txtMac.text = "Paired Device: $pairedMac"

        if (checkPermissionsGranted() && pairedMac != "None") {
            startService()
        }

        btnSync.setOnClickListener {
            DeskBuddyService.instance?.syncTime()
            Toast.makeText(this, "Clock Synced!", Toast.LENGTH_SHORT).show()
        }

        btnFind.setOnClickListener {
            DeskBuddyService.instance?.findDeskBuddy()
            Toast.makeText(this, "Buzzer Paged!", Toast.LENGTH_SHORT).show()
        }

        btnOta.setOnClickListener {
            val intent = Intent(Intent.ACTION_GET_CONTENT).apply {
                type = "*/*"
            }
            selectFileLauncher.launch(intent)
        }
    }

    private fun startService() {
        val intent = Intent(this, DeskBuddyService::class.java)
        ContextCompat.startForegroundService(this, intent)
        txtStatus.text = "Connection Service: Active"
    }

    private fun checkAndRequestPermissions() {
        val missing = requiredPermissions.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }
        if (missing.isNotEmpty()) {
            ActivityCompat.requestPermissions(this, missing.toTypedArray(), 101)
        }
    }

    private fun checkPermissionsGranted(): Boolean {
        return requiredPermissions.all {
            ContextCompat.checkSelfPermission(this, it) == PackageManager.PERMISSION_GRANTED
        }
    }

    private fun startOtaUpload(uri: Uri) {
        val service = DeskBuddyService.instance
        if (service == null || !service.isBleConnected()) {
            Toast.makeText(this, "Ensure Desk Buddy is connected first!", Toast.LENGTH_SHORT).show()
            return
        }
        try {
            val inputStream: InputStream? = contentResolver.openInputStream(uri)
            inputStream?.let { stream ->
                val bytes = stream.readBytes()
                stream.close()
                Toast.makeText(this, "Uploading Firmware (${bytes.size} bytes)...", Toast.LENGTH_LONG).show()
                service.performOtaUpdate(bytes) { progress ->
                    runOnUiThread {
                        btnOta.text = "OTA: $progress%"
                        if (progress == 100) {
                            btnOta.text = "Upload Complete"
                        }
                    }
                }
            }
        } catch (e: Exception) {
            Toast.makeText(this, "Failed to read binary file", Toast.LENGTH_SHORT).show()
        }
    }
}
