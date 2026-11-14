{
  "targets": [
    {
      "target_name": "aura_engine",
      "sources": [
        "addon.cpp",
        "AudioEngine.cpp",
        "AudioRenderer.cpp",
        "BufferTransfer.cpp",
        "RingBuffer.cpp",
        "wasapi.cpp"
      ],
      "include_dirs": [
        "node_modules/node-addon-api"
      ],
      "libraries": [
        "ole32.lib",
        "uuid.lib"
      ],
      "defines": [
        "NAPI_DISABLE_CPP_EXCEPTIONS",
        "UNICODE",
        "_UNICODE"
      ],
      "msvs_settings": {
        "VCCLCompilerTool": {
          "ExceptionHandling": 1,
          "AdditionalOptions": [ "/std:c++17" ]
        }
      }
    }
  ]
}