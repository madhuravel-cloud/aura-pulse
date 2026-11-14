#include "AudioEngine.h"
#include <future>
#include <napi.h>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>

static AudioEngine g_engine;

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

// Runs any AudioEngine call on a clean MTA thread isolated from Electron's STA
// UI thread
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
  for (size_t i = 0; i < devices.size(); ++i) {
    Napi::Object obj = Napi::Object::New(env);
    obj.Set("id", WideToUtf8(devices[i].id));
    obj.Set("name", WideToUtf8(devices[i].name));
    obj.Set("isDefault", devices[i].isDefault);
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
  for (size_t i = 0; i < statuses.size(); ++i) {
    Napi::Object obj = Napi::Object::New(env);
    obj.Set("id", WideToUtf8(statuses[i].id));
    obj.Set("name", WideToUtf8(statuses[i].name));
    obj.Set("active", statuses[i].active);
    obj.Set("statusMessage", statuses[i].statusMessage);
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

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("getOutputDevices", Napi::Function::New(env, GetOutputDevices));
  exports.Set("start", Napi::Function::New(env, StartEngine));
  exports.Set("stop", Napi::Function::New(env, StopEngine));
  exports.Set("isRunning", Napi::Function::New(env, IsEngineRunning));
  return exports;
}

NODE_API_MODULE(aura_engine, Init)