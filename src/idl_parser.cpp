#include <algorithm>
#include <list>

#include "pomeloc/idl.h"
#include "pomeloc/util.h"

namespace pomeloc {

#define GEN_TYPES_SCALAR(TD) \
  TD(NONE,   "",       byte) \
  TD(INT,    "int32",    int) \
  TD(UINT,   "uInt32",   uint) \
  TD(SINT,   "sInt32",   int) \
  TD(FLOAT,  "float",  float) \
  TD(DOUBLE, "double", double) \
  TD(STRING, "string", string) \
  TD(MESSAGE,  "message",  message)

    const char* const kTypeNames[] = {
        "int32",
        "uInt32",
        "sInt32",
        "float",
        "double",
        "string"
    };

    const char* const kOptionNames[] = {
        "required",
        "optional",
        "repeated"
    };

#define GEN_LOOKUP_TABLE(name, type, max, strarr) \
    static type name(const char* key) \
    {\
        for (int i = 0; i < max; ++i)\
        {\
            if (strcmp(strarr[i], key) == 0)\
            {\
                return (type)i;\
            }\
        }\
        return max;\
    }
    GEN_LOOKUP_TABLE(LookupType, kType, kTypeNone, kTypeNames)
    GEN_LOOKUP_TABLE(LookupOpt, MetaTypeOpt, kOptNone, kOptionNames)

    inline CheckedError NoError() { return CheckedError(false); }

    CheckedError Parser::ParseVariable(std::unordered_map<std::string, MetaStruct>& lookupt,
        std::vector<MetaVariable>& ret, const char* key, json& val)
    {
        std::vector<pomeloc::sslice> splits;
        pomeloc::strslice(key, 0, splits, " ");

        if (!val.is_number_integer())
        {
            return Error("error grammar " + std::string(key));
        }
        MetaVariable mv;
        auto optType = LookupOpt(std::string(splits.at(0).ptr, splits.at(0).sz).c_str());
        if (kOptNone == optType)
        {
            return Error("error type opt " + std::string(splits.at(0).ptr, splits.at(0).sz));
        }

        auto type = LookupType(std::string(splits.at(1).ptr, splits.at(1).sz).c_str());
        if (kTypeNone == type)
        {
            //if message type
            auto itFind = lookupt.find(std::string(splits.at(1).ptr, splits.at(1).sz));
            if (lookupt.end() == itFind)
            {
                return Error("error type " + std::string(splits.at(1).ptr, splits.at(1).sz));
            }
            mv.typename_ = itFind->first;
            type = kMessage;
        }
        mv.name_ = std::string(splits.at(2).ptr, splits.at(2).sz);
        mv.type_ = type;
        mv.opt_ = optType;
        mv.index_ = val;

        ret.push_back(mv);

        return NoError();
    }

    CheckedError Parser::ParseStruct(std::unordered_map<std::string, MetaStruct>& lookupt, 
        const char* key, json& val)
    {
        if (!val.is_object())
        {
            return Error("error grammar " + std::string(key));
        }
        MetaStruct ms;
        std::vector<pomeloc::sslice> splits;
        pomeloc::strslice(key, 0, splits, " ");
        if (splits.size() != 2)
        {
            return Error("error grammar " + std::string(key));
        }
        if (std::string(splits.at(0).ptr, splits.at(0).sz).compare("message") != 0)
        {
            return Error("unknown declare key type, struct declare must be [message] key word"
                + std::string(splits.at(0).ptr, splits.at(0).sz));
        }
        ms.name_ = std::string(splits.at(1).ptr, splits.at(1).sz);
        if (lookupt.end() != lookupt.find(ms.name_))
        {
            return Error("duplicate message name at same namespace " + ms.name_);
        }
        for (json::iterator it = val.begin();
        it != val.end(); ++it)
        {
            splits.clear();
            pomeloc::strslice(it.key().c_str(), 0, splits, " ");
            if (splits.size() == 2) //struct declare
            {
                auto err = this->ParseStruct(ms.structs_, it.key().c_str(), it.value());
                if (err.Check())
                {
                    return err;
                }
            }
            else if (splits.size() == 3) //variable
            {
                auto err = this->ParseVariable(ms.structs_, ms.vars_, it.key().c_str(), it.value());
                if (err.Check())
                {
                    return err;
                }
            }
            else
            {
                return Error("error key " + it.key());
            }
        }
        lookupt[ms.name_] = ms;
        return NoError();
    }

    CheckedError Parser::ParseRoot(std::vector<RootStruct>& rss, const char* key, json& val)
    {
        RootStruct rs;
        std::vector<pomeloc::sslice> splits;
        pomeloc::strslice(key, 0, splits, ".");
        if (splits.size() == 3)
        {
            rs.ns_ = std::string(splits.at(0).ptr, splits.at(0).sz);
            rs.class_ = std::string(splits.at(1).ptr, splits.at(1).sz);
            rs.method_ = std::string(splits.at(2).ptr, splits.at(2).sz);
        }
        else
        {
            rs.method_ = key;
            rs.is_event_ = true;
            //return Error("error key name. " + std::string(key));
            //return NoError(); //skip server method
        }
        rs.router_ = key;
        
        for (json::iterator it = val.begin();
        it != val.end(); ++it)
        {
            splits.clear();
            pomeloc::strslice(it.key().c_str(), 0, splits, " ");
            if (splits.size() == 2) //struct declare
            {
                auto err =  this->ParseStruct(rs.structs_, it.key().c_str(), it.value());
                if (err.Check())
                {
                    return err;
                }
            }
            else if (splits.size() == 3) //variable
            {
                auto err = this->ParseVariable(rs.structs_, rs.vars_, it.key().c_str(), it.value());
                if (err.Check())
                {
                    return err;
                }
            }
            else
            {
                return Error("error key " + it.key());
            }
        }

        std::sort(rs.vars_.begin(), rs.vars_.end(), 
            [](const MetaVariable& l, const MetaVariable& r) -> bool {
            return l.index_ < r.index_;
        });
        rss.push_back(rs);
        return NoError();
    }

    CheckedError Parser::Error(const std::string &msg) 
    {
        error_ += "error: " + msg;
        error_ += "\n";
        return CheckedError(true);
    }

    bool Parser::Parse(const char *source, const char *source_filename)
    {
        return !this->DoParse(source, source_filename).Check();
    }

    CheckedError Parser::DoParse(const char *source, const char *source_filename)
    {
        if (!source || !source_filename)
        {
            return Error("invalidate argument.");
        }
        json_content_ = json::parse(source);
        if (json_content_.empty())
        {
            return Error("parse error.  " + std::string(source_filename));
        }
        if (!json_content_.is_object())
        {
            return Error("the root data must be object type. " + std::string(source_filename));
        }

        structs_.clear();
        for (json::iterator it = json_content_.begin();
        it != json_content_.end(); ++it)
        {
            if (!(*it).is_object())
            {
                return Error("message data should be object type. " + std::string(source_filename));
            }
            if (this->ParseRoot(structs_, it.key().c_str(), it.value()).Check())
            {
                return Error("parse failed. " + it.key());
            }
        }
       
        return NoError();
    }
}  // namespace 
