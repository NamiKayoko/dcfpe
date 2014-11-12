#include "dpe_service/main/compiler/compiler_impl.h"

namespace ds
{

extern const ProgrammeLanguage  PL_UNKNOWN = "";
extern const ProgrammeLanguage  PL_C       = "c";
extern const ProgrammeLanguage  PL_CPP     = "cpp";
extern const ProgrammeLanguage  PL_PYTHON  = "python";
extern const ProgrammeLanguage  PL_JAVA    = "java";
extern const ProgrammeLanguage  PL_HASKELL = "haskell";

extern const ISArch             ARCH_UNKNOWN = "";
extern const ISArch             ARCH_X86     = "x86";
extern const ISArch             ARCH_X64     = "x64";

CompileJob::CompileJob() :
  language_(PL_UNKNOWN),
  optimization_option_(2),
  callback_(NULL)
{
}

CompileJob::~CompileJob()
{

}

BasicCompiler::BasicCompiler(const CompilerConfiguration& context) :
  curr_job_(NULL),
  context_(context),
  weakptr_factory_(this)
{

}

BasicCompiler::~BasicCompiler()
{
  if (compile_process_)
  {
    compile_process_->Stop();
  }
}

void BasicCompiler::OnStop(process::Process* p, process::ProcessContext* exit_code)
{
  if (!curr_job_) return;
  
  LOG(INFO) << "compiler output:\n" << curr_job_->compiler_output_;
  
  GenerateCmdline(curr_job_);
  
  auto cj = curr_job_;

  curr_job_ = NULL;
  compile_process_ = NULL;

  if (cj->callback_) cj->callback_->OnCompileFinished(cj);
  
}

void BasicCompiler::OnOutput(process::Process* p, bool is_std_out, const std::string& data)
{
  if (curr_job_)
  {
    curr_job_->compiler_output_.append(data.begin(), data.end());
  }
}

void BasicCompiler::ReportCompileSuccess(base::WeakPtr<BasicCompiler> self)
{
  if (auto* pThis = self.get())
  {
    pThis->OnStop(NULL, NULL);
  }
}

void BasicCompiler::ScheduleReportCompileSuccess()
{
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
      base::Bind(&BasicCompiler::ReportCompileSuccess,
            weakptr_factory_.GetWeakPtr()));
}

LanguageDetail BasicCompiler::GetLanguageDetail(const ProgrammeLanguage& language) const
{
  for (auto& iter: context_.language_detail_)
  {
    if (iter.language_ == language) return iter;
  }
  return LanguageDetail();
}

bool BasicCompiler::PreProcessJob(CompileJob* job)
{
  if (!job) return false;

  if (job->source_files_.empty()) return false;
  if (job->language_ == PL_UNKNOWN) job->language_ = DetectLanguage(job->source_files_);
  if (job->language_ == PL_UNKNOWN) return false;
  if (!context_.Accept(job->language_)) return false;
  
  if (job->output_file_.empty())
  {
    auto language_detail = GetLanguageDetail(job->language_);
    std::map<std::string, std::string> kv;
    kv["$(COMPILE_BINARY_DIR)"] = base::NativeToUTF8(context_.compile_binary_dir_.value());
    kv["$(OUTPUT_FILE)"] = base::NativeToUTF8(job->output_file_.value());
    kv["$(SOURCE_FILE_NAME)"] = base::NativeToUTF8(job->source_files_[0].BaseName().RemoveExtension().value());
    kv["$(SOURCE_FILE_PATH)"] = base::NativeToUTF8(job->source_files_[0].value());
    job->output_file_ = base::FilePath(base::UTF8ToNative(FixString(language_detail.default_output_file_, kv)));
  }
  
  return true;
}

std::string BasicCompiler::FixString(const std::string& s, const std::map<std::string, std::string>& kv)
{
  std::string ret = s;
  for (auto& iter: kv)
  {
    ReplaceSubstringsAfterOffset(&ret, 0, iter.first, iter.second);
  }
  return ret;
}

void BasicCompiler::FillOutputFile(CompileJob* job)
{
}

bool BasicCompiler::GenerateCmdline(CompileJob* job)
{
  if (!PreProcessJob(job)) return false;

  auto language_detail = GetLanguageDetail(job->language_);
  std::map<std::string, std::string> kv;
  kv["$(COMPILE_BINARY_DIR)"] = base::NativeToUTF8(context_.compile_binary_dir_.value());
  kv["$(OUTPUT_FILE)"] = base::NativeToUTF8(job->output_file_.value());
  kv["$(SOURCE_FILE_NAME)"] = base::NativeToUTF8(job->source_files_[0].BaseName().RemoveExtension().value());
  kv["$(SOURCE_FILE_PATH)"] = base::NativeToUTF8(job->source_files_[0].value());
  job->image_path_ = base::FilePath(base::UTF8ToNative(FixString(language_detail.running_binary_, kv)));

  job->arguments_.clear();
  for (auto& iter: language_detail.running_args_)
  {
    job->arguments_.push_back(base::UTF8ToNative(FixString(iter, kv)));
  }
  
  return true;
}

MingwCompiler::MingwCompiler(const CompilerConfiguration& context) :
  BasicCompiler(context)
{

}

MingwCompiler::~MingwCompiler()
{

}

void MingwCompiler::FillOutputFile(CompileJob* job)
{
  if (job->output_file_.empty())
  {
    job->output_file_ = job->source_files_[0].BaseName().ReplaceExtension(L".exe");
  }
}

bool MingwCompiler::StartCompile(CompileJob* job)
{
  if (!job || curr_job_) return false;

  if (!PreProcessJob(job)) return false;

  auto language_detail = GetLanguageDetail(job->language_);

  compile_process_ = new process::Process(this);

  auto& po = compile_process_->GetProcessOption();
  po.image_path_ = context_.compile_binary_dir_.Append(language_detail.compile_binary_.value());
  po.inherit_env_var_ = true;
  po.env_var_keep_ = context_.env_var_keep_;
  po.env_var_merge_ = context_.env_var_merge_;
  po.env_var_replace_ = context_.env_var_replace_;
  po.current_directory_ = job->current_directory_;

  po.argument_list_.clear();
  for (auto& it : job->source_files_) po.argument_list_.push_back(it.value());

  po.argument_list_.push_back(L"-o");
  po.argument_list_.push_back(job->output_file_.value());

  if (job->optimization_option_ > 0)
  {
    if (job->optimization_option_ == 1)
    {
      po.argument_list_.push_back(L"-O1");
    }
    else if (job->optimization_option_ == 2)
    {
      po.argument_list_.push_back(L"-O2");
    }
    else
    {
      po.argument_list_.push_back(L"-O3");
    }
  }
  
  for (auto& it: language_detail.compile_args_)
  {
    po.argument_list_.push_back(base::UTF8ToNative(it));
  }
  
  if (!job->cflags_.empty())
  {
    po.argument_list_r_.push_back(job->cflags_);
  }
  
  std::string().swap(job->compiler_output_);

  po.redirect_std_inout_ = true;
  po.treat_err_as_out_ = true;
  po.create_sub_process_ = true;
  po.treat_err_as_out_ = true;
  po.allow_sub_process_breakaway_job_ = true;

  bool ret = compile_process_->Start();
  if (ret)
  {
    curr_job_ = job;
    job->compile_process_ = compile_process_;
    LOG(INFO) << "compile command:\n" << base::SysWideToUTF8(compile_process_->GetProcessContext()->cmd_line_);
  }
  else
  {
    compile_process_ = NULL;
  }

  return ret;
}

VCCompiler::VCCompiler(const CompilerConfiguration& context) :
  BasicCompiler(context)
{

}

VCCompiler::~VCCompiler()
{

}

void VCCompiler::FillOutputFile(CompileJob* job)
{
  if (job->output_file_.empty())
  {
    job->output_file_ = job->current_directory_.Append(
                          job->source_files_[0].BaseName().ReplaceExtension(L".exe")
                        );
  }
}

bool VCCompiler::StartCompile(CompileJob* job)
{
  if (!job || curr_job_) return false;

  if (!PreProcessJob(job)) return false;

  auto language_detail = GetLanguageDetail(job->language_);

  compile_process_ = new process::Process(this);

  auto& po = compile_process_->GetProcessOption();
  po.image_path_ = context_.compile_binary_dir_.Append(language_detail.compile_binary_.value());
  po.inherit_env_var_ = true;
  po.env_var_keep_ = context_.env_var_keep_;
  po.env_var_merge_ = context_.env_var_merge_;
  po.env_var_replace_ = context_.env_var_replace_;
  po.current_directory_ = job->current_directory_;

  po.argument_list_.clear();
  for (auto& it : job->source_files_) po.argument_list_.push_back(it.value());

  po.argument_list_.push_back(L"/Fe"+job->output_file_.value());

  if (job->optimization_option_ > 0)
  {
    if (job->optimization_option_ == 1)
    {
      po.argument_list_.push_back(L"/O1");
    }
    else
    {
      po.argument_list_.push_back(L"/O2");
    }
  }
  
  for (auto& it: language_detail.compile_args_)
  {
    po.argument_list_.push_back(base::UTF8ToNative(it));
  }
  
  if (!job->cflags_.empty())
  {
    po.argument_list_r_.push_back(job->cflags_);
  }
  std::string().swap(job->compiler_output_);

  po.redirect_std_inout_ = true;
  po.treat_err_as_out_ = true;
  po.create_sub_process_ = true;
  po.treat_err_as_out_ = true;
  po.allow_sub_process_breakaway_job_ = true;

  bool ret = compile_process_->Start();

  if (ret)
  {
    curr_job_ = job;
    job->compile_process_ = compile_process_;
    LOG(INFO) << "compile command:\n" << base::SysWideToUTF8(compile_process_->GetProcessContext()->cmd_line_);
  }
  else
  {
    compile_process_ = NULL;
  }

  return ret;
}

GHCCompiler::GHCCompiler(const CompilerConfiguration& context) :
  BasicCompiler(context)
{

}

GHCCompiler::~GHCCompiler()
{

}

void GHCCompiler::FillOutputFile(CompileJob* job)
{
  if (job->output_file_.empty())
  {
    job->output_file_ = job->current_directory_.Append(
                          job->source_files_[0].BaseName().ReplaceExtension(L".exe")
                        );
  }
}

bool GHCCompiler::StartCompile(CompileJob* job)
{
  if (!job || curr_job_) return false;

  if (!PreProcessJob(job)) return false;

  auto language_detail = GetLanguageDetail(job->language_);

  compile_process_ = new process::Process(this);

  auto& po = compile_process_->GetProcessOption();
  po.image_path_ = context_.compile_binary_dir_.Append(language_detail.compile_binary_.value());
  po.inherit_env_var_ = true;
  po.env_var_keep_ = context_.env_var_keep_;
  po.env_var_merge_ = context_.env_var_merge_;
  po.env_var_replace_ = context_.env_var_replace_;
  po.current_directory_ = job->current_directory_;

  po.argument_list_.clear();
  for (auto& it : job->source_files_) po.argument_list_.push_back(it.value());

  po.argument_list_.push_back(L"-o");
  po.argument_list_.push_back(job->output_file_.value());
  
  for (auto& it: language_detail.compile_args_)
  {
    po.argument_list_.push_back(base::UTF8ToNative(it));
  }
  
#if 0
  if (job->optimization_option_ > 0)
  {
    if (job->optimization_option_ == 1)
    {
      po.argument_list_.push_back(L"/O1");
    }
    else if (job->optimization_option_ == 2)
    {
      po.argument_list_.push_back(L"/O2");
    }
    else
    {
      po.argument_list_.push_back(L"/O3");
    }
  }
#endif
  std::string().swap(job->compiler_output_);

  po.redirect_std_inout_ = true;
  po.treat_err_as_out_ = true;
  po.create_sub_process_ = true;
  po.treat_err_as_out_ = true;
  po.allow_sub_process_breakaway_job_ = true;

  bool ret = compile_process_->Start();
  if (ret)
  {
    curr_job_ = job;
    job->compile_process_ = compile_process_;
    LOG(INFO) << "compile command:\n" << base::SysWideToUTF8(compile_process_->GetProcessContext()->cmd_line_);
  }
  else
  {
    compile_process_ = NULL;
  }

  return ret;
}

PythonCompiler::PythonCompiler(const CompilerConfiguration& context) :
  BasicCompiler(context)
{

}

PythonCompiler::~PythonCompiler()
{

}

void PythonCompiler::FillOutputFile(CompileJob* job)
{
  if (job->output_file_.empty())
  {
    job->output_file_ = job->source_files_[0];
  }
}

bool PythonCompiler::StartCompile(CompileJob* job)
{
  if (!job || curr_job_) return false;
  
  GenerateCmdline(job);

  curr_job_ = job;

  ScheduleReportCompileSuccess();

  return true;
}

}

namespace ds
{
struct Ext2Lang
{
  const NativeChar* ext;
  const ProgrammeLanguage    lang;
};

static Ext2Lang info[] = 
{
{L".c", PL_C},
{L".cpp", PL_CPP},
{L".cc", PL_CPP},
{L".hpp", PL_CPP},
{L".py", PL_PYTHON},
{L".java", PL_JAVA},
{L".hs", PL_HASKELL},
};

ProgrammeLanguage DetectLanguage(const std::vector<base::FilePath>& filepath)
{
  static const int32_t info_size = sizeof(info) / sizeof(info[0]);
  for (auto& item: filepath)
  {
    NativeString ext = item.Extension();
    for (auto& it : info)
    {
      if (base::StringEqualCaseInsensitive(ext, it.ext))
      {
        return it.lang;
      }
    }
  }
  return PL_UNKNOWN;
}

bool CompilerConfiguration::Accept(const ProgrammeLanguage& language) const
{
  for (auto& it: language_detail_)
  if (it.language_ == language) return true;
  return false;
}

}
