#include "pomeloc/pomeloc.h"
#include "pomeloc/idl.h"
#include "pomeloc/util.h"
#include <algorithm>

namespace pomeloc {

// Convert an underscore_based_indentifier in to camelCase.
// Also uppercases the first character if first is true.
std::string MakeCamel(const std::string &in, bool first) {
  std::string s;
  for (size_t i = 0; i < in.length(); i++) {
    if (!i && first)
      s += static_cast<char>(toupper(in[0]));
    else if (in[i] == '_' && i + 1 < in.length())
      s += static_cast<char>(toupper(in[++i]));
    else
      s += in[i];
  }
  return s;
}

struct CommentConfig {
  const char *first_line;
  const char *content_line_prefix;
  const char *last_line;
};

// Generate a documentation comment, if available.
void GenComment(const std::vector<std::string> &dc, std::string *code_ptr,
                const CommentConfig *config, const char *prefix) {
  if (dc.begin() == dc.end()) {
    // Don't output empty comment blocks with 0 lines of comment content.
    return;
  }

  std::string &code = *code_ptr;
  if (config != nullptr && config->first_line != nullptr) {
    code += std::string(prefix) + std::string(config->first_line) + "\n";
  }
  std::string line_prefix = std::string(prefix) +
      ((config != nullptr && config->content_line_prefix != nullptr) ?
       config->content_line_prefix : "///");
  for (auto it = dc.begin();
       it != dc.end();
       ++it) {
    code += line_prefix + *it + "\n";
  }
  if (config != nullptr && config->last_line != nullptr) {
    code += std::string(prefix) + std::string(config->last_line) + "\n";
  }
}

// These arrays need to correspond to the IDLOptions::k enum.

struct LanguageParameters {
  IDLOptions::Language language;
  // Whether function names in the language typically start with uppercase.
  bool first_camel_upper;
  const char *file_extension;
  const char *string_type;
  const char *bool_type;
  const char *open_curly;
  const char *const_decl;
  const char *unsubclassable_decl;
  const char *enum_decl;
  const char *enum_separator;
  const char *getter_prefix;
  const char *getter_suffix;
  const char *inheritance_marker;
  const char *namespace_ident;
  const char *namespace_begin;
  const char *namespace_end;
  const char *set_bb_byteorder;
  const char *get_bb_position;
  const char *get_fbb_offset;
  const char *includes;
  CommentConfig comment_config;
};

LanguageParameters language_parameters[] = {
  {
    IDLOptions::kCSharp,
    true,
    ".cs",
    "string",
    "bool ",
    "\n{\n",
    " readonly ",
    "sealed ",
    "enum ",
    ",\n",
    " { get",
    "} ",
    " : ",
    "namespace ",
    "\n{",
    "\n}\n",
    "",
    "Position",
    "Offset",
    "using System;\nusing LitJson;\nusing Pomelo.DotNetClient;\n",
    {
      nullptr,
      "///",
      nullptr,
    },
  }
};

static_assert(sizeof(language_parameters) / sizeof(LanguageParameters) ==
    IDLOptions::kMAX,
    "Please add extra elements to the arrays above.");

// Save out the generated code for a single class while adding
// declaration boilerplate.
static bool SaveClass(const LanguageParameters &lang, const Parser &parser,
    const std::string &defname, const std::string &classcode,
    const std::string &path, bool needs_includes, bool onefile) {
    if (!classcode.length()) return true;

    EnsureDirExists(path);

    std::string code = "";// "// automatically generated, do not modify\n\n";
    if (needs_includes) code += lang.includes;
    code += classcode;
    auto filename = path + defname + lang.file_extension;
    return SaveFile(filename.c_str(), code, false);
}

const char* const kTypeCsharp[] = {
    "int",
    "int",
    "int",
    "float",
    "double",
    "string"
};

const char* const kTypeCsharpDefault[] = {
    "0",
    "0",
    "0",
    "0.0f",
    "0.0",
    "\"\"",
};

inline const char* MaptoTypeDefaultString(kType t)
{
    if (t >= 0 && t < kTypeNone)
    {
        return kTypeCsharpDefault[t];
    }
    return "null";
}

inline const char* MaptoTypeString(const MetaVariable& mv)
{
    if (mv.type_ >= 0 && mv.type_ < kTypeNone)
    {
        return kTypeCsharp[mv.type_];
    }
    return mv.typename_.c_str();
}

static void GenMetaVariable(const LanguageParameters &lang, const Parser &parser,
    const MetaVariable& mv, std::string& code)
{
    code += "public ";
    switch (mv.opt_)
    {
    case kRequired:
    case kOptional:
        {
            code += MaptoTypeString(mv);
            code += " ";
            code += mv.name_;
            code += ";";
        }
        break;

    case kRepeated:
        {
            code += MaptoTypeString(mv);
            code += "[] ";
            code += mv.name_;
            code += ";";
        }
        break;
    default:
        break;
    }
}

static std::string GenMethodToJsonBodyArray(const std::string& name, kType t)
{
    std::string body;
    if (t == kMessage)
    {
        body += "if(";
        body += name;
        body += " != null){";
        body += "data[\"";
        body += name;
        body += "\"] = new JsonData();";
        body += "for(int i=0;i<";
        body += name;
        body += ".Length;++i){";
        body += "data[\"";
        body += name;
        body += "\"].Add(";
        body += name;
        body += "[i].ToJson());";
        body += "}";
        body += "}";
    }
    else
    {
        body += "for(int i=0;i<";
        body += name;
        body += ".Length;++i){";
        body += "data[\"";
        body += name;
        body += "\"].Add(";
        body += name;
        body += "[i]);";
        body += "}";
    }
    
    return body;
}

static std::string GenMethodFromJsonBodyArray(const MetaVariable& mv, const char* varname, const char* ns = nullptr)
{
    std::string body;
    if (mv.type_ == kMessage)
    {
        body += "if(ret.ContainsKey(\"";
        body += mv.name_;
        body += "\") && ret[\"";
        body += mv.name_;
        body += "\"].IsArray && ret[\"";
        body += mv.name_;
        body += "\"].Count > 0){";
        body += varname;
        body += ".";
        body += mv.name_;
        body += " = new ";
        if (ns)
        {
            body += ns;
            body += ".";
        }
        body += MaptoTypeString(mv);
        body += "[ret[\"";
        body += mv.name_;
        body += "\"].Count];";
        body += "for(int i=0;i<ret[\"";
        body += mv.name_;
        body += "\"].Count;++i){";
        body += varname;
        body += ".";
        body += mv.name_;
        body += "[i] = new ";
        if (ns)
        {
            body += ns;
            body += ".";
        }
        body += MaptoTypeString(mv);
        body += "();";
        body += varname;
        body += ".";
        body += mv.name_;
        body += "[i].FromJson(ret[\"";
        body += mv.name_;
        body += "\"][i]);";
        body += "}}";
    }
    else
    {
        body += "if(ret.ContainsKey(\"";
        body += mv.name_;
        body += "\") && ret[\"";
        body += mv.name_;
        body += "\"].IsArray && ret[\"";
        body += mv.name_;
        body += "\"].Count > 0){";
        body += varname;
        body += ".";
        body += mv.name_;
        body += " = new ";
        body += MaptoTypeString(mv);
        body += "[ret[\"";
        body += mv.name_;
        body += "\"].Count];";
        body += "for(int i=0;i<ret[\"";
        body += mv.name_;
        body += "\"].Count;++i){";
        body += varname;
        body += ".";
        body += mv.name_;
        body += "[i]=(";
        body += MaptoTypeString(mv);
        body += ")ret[\"";
        body += mv.name_;
        body += "\"][i];";
        body += "}}";
    }

    return body;
}

static std::string GenMethodToJsonBody(const LanguageParameters &lang, const Parser &parser,
    const std::vector<MetaVariable>& vars)
{
    std::string body;
    for (const auto& item : vars)
    {
        if (item.type_ == kMessage)
        {
            if (item.opt_ == kRepeated)
            {
                body += GenMethodToJsonBodyArray(item.name_, item.type_);
            }
            else if (item.opt_ == kOptional)
            {
                body += "if(";
                body += item.name_;
                body += " != null){data[\"";
                body += item.name_;
                body += "\"]=";
                body += item.name_;
                body += ".ToJson();}";
            }
            else
            {
                body += "data[\"";
                body += item.name_;
                body += "\"]=";
                body += item.name_;
                body += ".ToJson();";
            }
        }
        else
        {
            if (item.opt_ == kRepeated)
            {
                body += GenMethodToJsonBodyArray(item.name_, item.type_);
            }
            else
            {
                body += "data[\"";
                body += item.name_;
                body += "\"] = ";
                body += item.name_;
                body += ";";
            }
        }
    }
    return body;
}

static std::string GenMethodFromJsonBody(const LanguageParameters &lang, const Parser &parser,
    const std::vector<MetaVariable>& vars, const char* varname, const char* ns = nullptr)
{
    std::string body;
    for (const auto& item : vars)
    {
        if (item.type_ == kMessage)
        {
            if (item.opt_ == kRepeated)
            {
                body += GenMethodFromJsonBodyArray(item, varname, ns);
            }
            else
            {
                body += "if(ret.ContainsKey(\"";
                body += item.name_;
                body += "\")){";
                body += varname;
                body += ".";
                body += item.name_;
                body += " = new ";
                if (ns)
                {
                    body += ns;
                    body += ".";
                }
                body += MaptoTypeString(item);
                body += "();";
                body += varname;
                body += ".";
                body += item.name_;
                body += ".FromJson(ret[\"";
                body += item.name_;
                body += "\"]);}";
            }
        }
        else
        {
            if (item.opt_ == kRepeated)
            {
                body += GenMethodFromJsonBodyArray(item, varname);
            }
            else
            {
                body += varname;
                body += ".";
                body += item.name_;
                body += "= ret.ContainsKey(\"";
                body += item.name_;
                body += "\")?(";
                body += MaptoTypeString(item);
                body += ")ret[\"";
                body += item.name_;
                body += "\"]:";
                body += MaptoTypeDefaultString(item.type_);
                body += ";";
            }
        }
    }
    return body;
}

static void GenMethodToJson(const LanguageParameters &lang, const Parser &parser,
    const MetaStruct& ms, std::string& code)
{
    code += "public JsonData ToJson(){JsonData data = new JsonData();";
    code += GenMethodToJsonBody(lang, parser, ms.vars_);
    code += "return data;}";
}

static void GenMethodFromJson(const LanguageParameters &lang, const Parser &parser,
    const MetaStruct& ms, std::string& code)
{
    code += "public void FromJson(JsonData ret){";
    code += GenMethodFromJsonBody(lang, parser, ms.vars_, "this");
    code += "}";
}

static void GenMetaStruct(const LanguageParameters &lang, const Parser &parser,
    const MetaStruct& ms, std::string& code)
{
    code += "public class ";
    code += ms.name_;
    code += "{";

    for (const auto& item : ms.structs_)
    {
        GenMetaStruct(lang, parser, item.second, code);
    }

    for (const auto& item : ms.vars_)
    {
        GenMetaVariable(lang, parser, item, code);
    }

    //generator JsonData Serialized Method
    std::string mtojson, mfromjson;
    GenMethodToJson(lang, parser, ms, mtojson);
    GenMethodFromJson(lang, parser, ms, mfromjson);

    code += mtojson;
    code += mfromjson;
    code += "}";
}

static void GenFuncArguments(const LanguageParameters &lang, const Parser &parser,
    const RootStruct& rs, std::string& code)
{
    code += "(";
    std::string reqArg, optArg;
    for (const auto& item : rs.vars_)
    {
        switch (item.opt_)
        {
        case kRequired:
        {
            reqArg += MaptoTypeString(item);
            reqArg += " ";
            reqArg += item.name_;
            reqArg += ",";
        }
        break;
        case kOptional:
        {
            optArg += MaptoTypeString(item);
            optArg += " ";
            optArg += item.name_;
            if (item.opt_ == kOptional)
            {
                optArg += "=";
                optArg += MaptoTypeDefaultString(item.type_);
            }
            optArg += ",";
        }
        break;

        case kRepeated:
        {
            reqArg += MaptoTypeString(item);
            reqArg += "[] ";
            reqArg += item.name_;
            reqArg += ",";
        }
        break;
        default:
            break;
        }
    }
    code += reqArg;
    code += optArg;
    {
        auto itResponse = parser.response_maps_.find(rs.router_);
        if (itResponse != parser.response_maps_.end())
        {
            code += "System.Action<";
            code += itResponse->second.name_;
            code += "> cb,";
        }
    }

    if (code.at(code.size() - 1) == '(')
    {
        code += ")";
    }
    else
    {
        code[(code.size() - 1)] = ')';
    }
}

static std::string SerializeJson(const MetaVariable& var)
{
    std::string code;

    return code;
}

static std::string GenResponseCallBackBody(const LanguageParameters &lang, const Parser &parser,
    const RootStruct& rs, const MetaStruct& ms)
{
    std::string code;
    code += ms.name_;
    code += " result = new ";
    code += ms.name_;
    code += "();";
    code += GenMethodFromJsonBody(lang, parser, ms.vars_, "result", ms.name_.c_str());
    code += "cb(result);";
    return code;
}

static void GenEventFuncBody(const LanguageParameters &lang, const Parser &parser,
    const RootStruct& rs, std::string& code, const MetaStruct& msevent)
{
    code += "{";

    code += "pc.on(\"";
    code += rs.router_;
    code += "\", delegate (JsonData ret){";
    
    code += msevent.name_;
    code += " result = new ";
    code += msevent.name_;
    code += "();";
    for (const auto& var : msevent.vars_)
    {
        if (var.type_ == kMessage)
        {
            code += "if(ret.ContainsKey(\"";
            code += var.name_;
            code += "\")){";
            if (var.opt_ == kRepeated)
            {
                code += "if(ret[\"";
                code += var.name_;
                code += "\"].IsArray && ret[\"";
                code += var.name_;
                code += "\"].Count > 0){";
                code += "result.";
                code += var.name_;
                code += " = new ";
                code += msevent.name_;
                code += ".";
                code += MaptoTypeString(var);
                code += "[ret[\"";
                code += var.name_;
                code += "\"].Count];";
                code += "for(int i=0;i<ret[\"";
                code += var.name_;
                code += "\"].Count;++i){";
                code += "result.";
                code += var.name_;
                code += "[i] = new ";
                code += msevent.name_;
                code += ".";
                code += MaptoTypeString(var);
                code += "();result.";
                code += var.name_;
                code += "[i].FromJson(";
                code += "ret[\"";
                code += var.name_;
                code += "\"][i]);";
                code += "}";
                code += "}";
            }
            else
            {
                code += "result.";
                code += var.name_;
                code += " = new ";
                code += msevent.name_;
                code += ".";
                code += MaptoTypeString(var);
                code += "();";
                code += "result.";
                code += var.name_;
                code += ".FromJson(";
                code += "ret[\"";
                code += var.name_;
                code += "\"]);";
            }
            
            code += "}";
        }
        else
        {
            code += "if(ret.ContainsKey(\"";
            code += var.name_;
            code += "\")){";

            if (var.opt_ == kRepeated)
            {
                code += "if(ret[\"";
                code += var.name_;
                code += "\"].IsArray && ret[\"";
                code += var.name_;
                code += "\"].Count > 0){";
                code += "result.";
                code += var.name_;
                code += " = new ";
                code += MaptoTypeString(var);
                code += "[ret[\"";
                code += var.name_;
                code += "\"].Count];";
                code += "for(int i=0;i<ret[\"";
                code += var.name_;
                code += "\"].Count;++i){";
                code += "result.";
                code += var.name_;
                code += "[i] = (";
                code += MaptoTypeString(var);
                code += ")ret[\"";
                code += var.name_;
                code += "\"][i];";
                code += "}";
                code += "}";
            }
            else
            {
                code += "result.";
                code += var.name_;
                code += " = (";
                code += MaptoTypeString(var);
                code += ")ret[\"";
                code += var.name_;
                code += "\"];";
            }
            code += "}";
        }
    }

    code += "cb(result);";
    code += "});";
    code += "return true;";

    code += "}";
}

static void GenFuncBody(const LanguageParameters &lang, const Parser &parser,
    const RootStruct& rs, std::string& code)
{
    code += "{";

    code += "JsonData data = new JsonData();";
    code += GenMethodToJsonBody(lang, parser, rs.vars_);

    auto itResponse = parser.response_maps_.find(rs.router_);
    if (itResponse != parser.response_maps_.end())
    {
        code += "pc.request(\"";
        code += rs.router_;
        code += "\", data, delegate (JsonData ret){";
        code += GenResponseCallBackBody(lang, parser, rs, itResponse->second);
        code += "});";
        code += "return true;";
    }
    else
    {
        code += "pc.notify(\"";
        code += rs.router_;
        code += "\", data);";
        code += "return true;";
    }
    
    code += "}";
}

static void GenEventStruct(const LanguageParameters &lang, const Parser &parser,
    const RootStruct& rs, std::string& code)
{
    MetaStruct ms;
    ms.name_ = rs.method_ + "_event";
    ms.structs_ = rs.structs_;
    ms.vars_ = rs.vars_;
    GenMetaStruct(lang, parser, ms, code);

    code += "public static bool ";
    code += rs.method_;
    {
        code += "(System.Action<";
        code += ms.name_;
        code += "> cb)";
    }
    std::string funcbody;
    GenEventFuncBody(lang, parser, rs, funcbody, ms);
    code += funcbody;
}

static void GenRootStruct(const LanguageParameters &lang, const Parser &parser,
    const RootStruct& rs, std::string& code) 
{
    for (const auto& item : rs.structs_)
    {
        GenMetaStruct(lang, parser, item.second, code);
    }

    {
        auto itResponse = parser.response_maps_.find(rs.router_);
        if (itResponse != parser.response_maps_.end())
        {
            GenMetaStruct(lang, parser, itResponse->second, code);
        }
    }

    code += "public static bool ";
    code += rs.method_;
    std::string arglist;
    GenFuncArguments(lang, parser, rs, arglist);
    code += arglist;
    std::string funcbody;
    GenFuncBody(lang, parser, rs, funcbody);
    code += funcbody;
}

std::string GenTabSpace(int n)
{
	std::string ret;
	for (int i = 0; i < 4*n; i++)
	{
		ret += " ";
	}
	return ret;
}

void Format(const std::string &code, std::string& fmt)
{
	int index = 0;
    for (size_t i = 0; i < code.length(); ++i)
    {
        char ch = code.at(i);
		if (ch == '{')
		{
			fmt += "\n";
            fmt += GenTabSpace(index);
			++index;
            fmt += ch;
            fmt += "\n";
			fmt += GenTabSpace(index);
            continue;
		}
		fmt += ch;
		if (ch == ';')
		{
			fmt += "\n";
            if (i + 1 < code.length() && code.at(i + 1) == '}')
            {
                fmt += GenTabSpace(index-1);
            }
            else
            {
                fmt += GenTabSpace(index);
            }
		}
		if (ch == '}')
		{
			fmt += "\n";
			--index;
            if (i + 1 < code.length() && code.at(i + 1) == '}')
            {
                fmt += GenTabSpace(index - 1);
            }
            else
            {
                fmt += GenTabSpace(index);
            }
		}
	}

	assert(0 == index && "mismatch {} !!!");
}

bool GenerateGeneral(const Parser& parser,
                     const std::string &path,
                     const std::string & file_name) 
{

  assert(parser.opts.lang <= IDLOptions::kMAX);
  auto lang = language_parameters[parser.opts.lang];
  std::string one_file_code;

  //group by ns, class ,method
  using G_BY_METHOD = std::unordered_map<std::string, pomeloc::RootStruct>;
  using G_BY_CLASS = std::unordered_map<std::string, G_BY_METHOD>;
  using G_BY_NS = std::unordered_map<std::string, G_BY_CLASS>;
  G_BY_NS tmpgroup;
  for (const auto& item : parser.structs_)
  {
	  auto it = tmpgroup.find(item.ns_);
	  if (it != tmpgroup.end())
	  {
		  auto iit = it->second.find(item.class_);
		  if (iit != it->second.end())
		  {
			  auto iiit = iit->second.find(item.method_);
			  if (iiit != iit->second.end())
			  {
				  return false;
			  }
			  iit->second[item.method_] = item;
		  }
		  else
		  {
			  it->second[item.class_][item.method_] = item;
		  }
	  }
	  else
	  {
		  tmpgroup[item.ns_][item.class_][item.method_] = item;
	  }

      auto& rs = tmpgroup[item.ns_][item.class_][item.method_];
      for (auto& ms : rs.structs_)
      {
          std::string olename = ms.second.name_;
          ms.second.name_ = item.method_ + "_" + ms.second.name_;
          for (auto& mv : rs.vars_)
          { 
              if (olename.compare(mv.typename_) == 0)
              {
                  mv.typename_ = ms.second.name_;
              }
          }
      }
  }

  std::string declcode;
  if (!parser.opts.custom_ns.empty())
  {
      declcode += "namespace ";
      declcode += parser.opts.custom_ns;
      declcode += "{";
  }
  for (auto& ns : tmpgroup)
  {
	  declcode += "namespace ";
	  declcode += ns.first;
	  declcode += "{";
	  for (auto& cls : ns.second)
	  {
		  declcode += "public class ";
		  declcode += cls.first;
		  declcode += "{";
		  declcode += "public static PomeloClient pc = null;";

		  for (auto& method : cls.second)
		  {
			  GenRootStruct(lang, parser, method.second, declcode);
		  }
		  declcode += "}";
	  }
	  declcode += "}";
  }
  declcode += "public class ServerEvent{public static PomeloClient pc = null;";
  for (const auto& item : parser.event_structs_)
  {
      GenEventStruct(lang, parser, item, declcode);
  }
  declcode += "}";
  if (!parser.opts.custom_ns.empty())
  {
      declcode += "}";
  }
  Format(declcode, one_file_code);
  return SaveClass(lang, parser, file_name, one_file_code,path, true, true);
}

}  // namespace pomeloc
