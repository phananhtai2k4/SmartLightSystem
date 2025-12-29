package com.example.smart_light_app

import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import com.example.smart_light_app.mesh.MeshPlugin

class MainActivity : FlutterActivity() {
    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)
        
        // Register MeshPlugin
        flutterEngine.plugins.add(MeshPlugin())
    }
}

