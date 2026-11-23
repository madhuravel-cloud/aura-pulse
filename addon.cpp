#include "AudioEngine.h"
#include <cstdint>
#include <future>
#include <mutex>
#include <napi.h>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>

static AudioEngine g_engine;
static std::mutex g_tsfnMutex;
static Napi::ThreadSafeFunction g_devicesChangedTsfn;
static bool g_hasTsfn = false;

static std::string WideToUtf8(const std::wstring &wstr) {
  if (wstr.empty())
    return "";
  int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0,
                                 nullptr, nullptr);
  if (size <= 0)
    return "";
  std::string result(size - 1, '\0');
  WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size, nullptr,
                      nullptr);
  return result;
}

static std::wstring Utf8ToWide(const std::string &str) {
  if (str.empty())
    return L"";
  int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
  if (size <= 0)
    return L"";
  std::wstring result(size - 1, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
  return result;
}

template <typename Func> auto RunOnComMtaThread(Func &&fn) -> decltype(fn()) {
  using RetType = decltype(fn());
  std::packaged_task<RetType()> task([&fn]() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if constexpr (std::is_void_v<RetType>) {
      fn();
      CoUninitialize();
    } else {
      RetType res = fn();
      CoUninitialize();
      return res;
    }
  });
  auto future = task.get_future();
  std::thread worker(std::move(task));
  worker.join();
  return future.get();
}

Napi::Value GetOutputDevices(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  auto devices =
      RunOnComMtaThread([]() { return g_engine.GetOutputDevices(); });

  Napi::Array jsArray = Napi::Array::New(env, devices.size());
  for (uint32_t i = 0; i < static_cast<uint32_t>(devices.size()); ++i) {
    Napi::Object obj = Napi::Object::New(env);
    obj.Set("id", Napi::String::New(env, WideToUtf8(devices[i].id)));
    obj.Set("name", Napi::String::New(env, WideToUtf8(devices[i].name)));
    obj.Set("isDefault", Napi::Boolean::New(env, devices[i].isDefault));
    jsArray.Set(i, obj);
  }
  return jsArray;
}

Napi::Value StartEngine(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  std::vector<std::wstring> targetIds;

  if (info.Length() > 0 && info[0].IsArray()) {
    Napi::Array arr = info[0].As<Napi::Array>();
    for (uint32_t i = 0; i < arr.Length(); ++i) {
      Napi::Value val = arr.Get(i);
      if (val.IsString()) {
        targetIds.push_back(Utf8ToWide(val.As<Napi::String>().Utf8Value()));
      }
    }
  }

  auto statuses =
      RunOnComMtaThread([&targetIds]() { return g_engine.Start(targetIds); });

  Napi::Array jsResults = Napi::Array::New(env, statuses.size());
  for (uint32_t i = 0; i < static_cast<uint32_t>(statuses.size()); ++i) {
    Napi::Object obj = Napi::Object::New(env);
    obj.Set("id", Napi::String::New(env, WideToUtf8(statuses[i].id)));
    obj.Set("name", Napi::String::New(env, WideToUtf8(statuses[i].name)));
    obj.Set("active", Napi::Boolean::New(env, statuses[i].active));
    obj.Set("statusMessage", Napi::String::New(env, statuses[i].statusMessage));
    jsResults.Set(i, obj);
  }
  return jsResults;
}

Napi::Value StopEngine(const Napi::CallbackInfo &info) {
  RunOnComMtaThread([]() { g_engine.Stop(); });
  return info.Env().Undefined();
}

Napi::Value IsEngineRunning(const Napi::CallbackInfo &info) {
  return Napi::Boolean::New(info.Env(), g_engine.IsRunning());
}

Napi::Value SetDeviceVolume(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  if (info.Length() < 2 || !info[0].IsString() || !info[1].IsNumber()) {
    return env.Undefined();
  }

  std::wstring deviceId = Utf8ToWide(info[0].As<Napi::String>().Utf8Value());
  float volume = info[1].As<Napi::Number>().FloatValue();

  g_engine.SetDeviceVolume(deviceId, volume);
  return env.Undefined();
}

Napi::Value SetDeviceDelay(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  if (info.Length() < 2 || !info[0].IsString() || !info[1].IsNumber()) {
    return env.Undefined();
  }

  std::wstring deviceId = Utf8ToWide(info[0].As<Napi::String>().Utf8Value());
  uint32_t delayMs = info[1].As<Napi::Number>().Uint32Value();

  g_engine.SetDeviceDelay(deviceId, delayMs);
  return env.Undefined();
}

Napi::Value OnDevicesChanged(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  if (info.Length() < 1 || !info[0].IsFunction()) {
    return env.Undefined();
  }

  std::lock_guard<std::mutex> lock(g_tsfnMutex);
  if (g_hasTsfn) {
    g_devicesChangedTsfn.Release();
    g_hasTsfn = false;
  }

  g_devicesChangedTsfn = Napi::ThreadSafeFunction::New(
      env, info[0].As<Napi::Function>(), "DevicesChangedCallback", 0, 1);
  g_devicesChangedTsfn.Unref(env);
  g_hasTsfn = true;

  g_engine.SetDevicesChangedCallback([]() {
    std::lock_guard<std::mutex> lock(g_tsfnMutex);
    if (g_hasTsfn) {
      g_devicesChangedTsfn.NonBlockingCall(
          [](Napi::Env env, Napi::Function jsCallback) {
            jsCallback.Call({});
          });
    }
  });

  return env.Undefined();
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("getOutputDevices", Napi::Function::New(env, GetOutputDevices));
  exports.Set("start", Napi::Function::New(env, StartEngine));
  exports.Set("stop", Napi::Function::New(env, StopEngine));
  exports.Set("isRunning", Napi::Function::New(env, IsEngineRunning));
  exports.Set("setVolume", Napi::Function::New(env, SetDeviceVolume));
  exports.Set("setDelay", Napi::Function::New(env, SetDeviceDelay));
  exports.Set("onDevicesChanged", Napi::Function::New(env, OnDevicesChanged));
  return exports;
}

NODE_API_MODULE(aura_engine, Init)