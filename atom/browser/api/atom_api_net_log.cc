// Copyright (c) 2018 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "atom/browser/api/atom_api_net_log.h"

#include <utility>

#include "atom/browser/atom_browser_main_parts.h"
#include "atom/common/native_mate_converters/callback.h"
#include "atom/common/native_mate_converters/file_path_converter.h"
#include "base/command_line.h"
#include "components/net_log/chrome_net_log.h"
#include "content/public/browser/browser_thread.h"
#include "native_mate/dictionary.h"
#include "native_mate/handle.h"
#include "net/url_request/url_request_context_getter.h"

#include "atom/common/node_includes.h"

namespace atom {

namespace api {

NetLog::NetLog(v8::Isolate* isolate) {
  Init(isolate);

  net_log_writer_ =
      atom::AtomBrowserMainParts::Get()->net_log()->net_export_file_writer();
  net_log_writer_->Initialize(content::BrowserThread::GetTaskRunnerForThread(
      content::BrowserThread::IO));
  net_log_writer_->AddObserver(this);
}

NetLog::~NetLog() {
  net_log_writer_->RemoveObserver(this);
}

// static
v8::Local<v8::Value> NetLog::Create(v8::Isolate* isolate) {
  return mate::CreateHandle(isolate, new NetLog(isolate)).ToV8();
}

void NetLog::StartLogging(mate::Arguments* args) {
  base::FilePath log_path;
  if (!args->GetNext(&log_path) || log_path.empty()) {
    args->ThrowError("The first parameter must be a valid string");
    return;
  }

  // TODO(deepak1556): Provide more flexibility to this module
  // by allowing customizations on the capturing options.
  net_log_writer_->StartNetLog(
      log_path, net::NetLogCaptureMode::Default(),
      net_log::NetExportFileWriter::kNoLimit /* file size limit */,
      base::CommandLine::ForCurrentProcess()->GetCommandLineString(),
      std::string(), {} /* URLRequestContextGetter list */);
}

bool NetLog::IsCurrentlyLogging() {
  const base::Value* current_log_state =
      net_log_state_->FindKeyOfType("state", base::Value::Type::STRING);
  if (current_log_state && current_log_state->GetString() == "LOGGING")
    return true;

  return false;
}

std::string NetLog::GetCurrentlyLoggingPath() {
  // Net log exporter has a default path which will be used
  // when no log path is provided, but since we don't allow
  // net log capture without user provided file path, this
  // check is completely safe.
  if (IsCurrentlyLogging()) {
    const base::Value* current_log_path =
        net_log_state_->FindKeyOfType("file", base::Value::Type::STRING);
    if (current_log_path)
      return current_log_path->GetString();
  }

  return std::string();
}

void NetLog::StopLogging(mate::Arguments* args) {
  args->GetNext(&stop_callback_);
  if (IsCurrentlyLogging()) {
    net_log_writer_->StopNetLog(nullptr, nullptr);
  } else {
    std::move(stop_callback_).Run();
  }
}

void NetLog::OnNewState(const base::DictionaryValue& state) {
  net_log_state_ = state.CreateDeepCopy();

  if (stop_callback_.is_null())
    return;

  const auto* current_log_state =
      state.FindKeyOfType("state", base::Value::Type::STRING);
  if (current_log_state && current_log_state->GetString() == "STOPPING_LOG")
    std::move(stop_callback_).Run();
}

// static
void NetLog::BuildPrototype(v8::Isolate* isolate,
                            v8::Local<v8::FunctionTemplate> prototype) {
  prototype->SetClassName(mate::StringToV8(isolate, "NetLog"));
  mate::ObjectTemplateBuilder(isolate, prototype->PrototypeTemplate())
      .SetProperty("currentlyLogging", &NetLog::IsCurrentlyLogging)
      .SetProperty("currentlyLoggingPath", &NetLog::GetCurrentlyLoggingPath)
      .SetMethod("startLogging", &NetLog::StartLogging)
      .SetMethod("_stopLogging", &NetLog::StopLogging);
}

}  // namespace api

}  // namespace atom

namespace {

using atom::api::NetLog;

void Initialize(v8::Local<v8::Object> exports,
                v8::Local<v8::Value> unused,
                v8::Local<v8::Context> context,
                void* priv) {
  v8::Isolate* isolate = context->GetIsolate();

  mate::Dictionary dict(isolate, exports);
  dict.Set("netLog", NetLog::Create(isolate));
  dict.Set("NetLog", NetLog::GetConstructor(isolate)->GetFunction());
}

}  // namespace

NODE_BUILTIN_MODULE_CONTEXT_AWARE(atom_browser_net_log, Initialize)
