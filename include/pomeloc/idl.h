#ifndef FLATBUFFERS_IDL_H_
#define FLATBUFFERS_IDL_H_

#include <map>
#include <unordered_map>
#include <set>
#include <stack>
#include <memory>
#include <functional>

#include "pomeloc/pomeloc.h"
#include "pomeloc/json.hpp"
using namespace nlohmann;

 // This file defines the data types representing a parsed IDL (Interface
 // Definition Language) / schema file.

namespace pomeloc
{
    enum MetaTypeOpt
    {
        kRequired = 0,
        kOptional,
        kRepeated, //array
        kOptNone,
    };

    enum kType
    {
        kInt32 = 0,
        kuInt32,
        ksInt32,
        kfloat,
        kdouble,
        kstring,
        kTypeNone,
        kMessage,
    };

    enum MetaResult
    {
        Variable = 0,
        Declare = 1,
    };

    typedef struct tagMetaVariable
    {
        int32_t index_;
        MetaTypeOpt opt_;
        kType type_;
        std::string name_;
        std::string typename_;
    }MetaVariable;

    typedef struct tagMetaStruct
    {
        std::string name_;
        std::string ns_;
        std::vector<MetaVariable> vars_;
        std::unordered_map<std::string, tagMetaStruct> structs_;
    }MetaStruct;
    typedef struct tagRootStruct
    {
        tagRootStruct()
            : is_event_(false)
        {

        }

		tagRootStruct(const tagRootStruct& it)
		{
			if (&it != this)
			{
				this->ns_ = it.ns_;
				this->class_ = it.class_;
				this->method_ = it.method_;
				this->router_ = it.router_;
				this->vars_ = it.vars_;
				this->structs_ = it.structs_;
                this->is_event_ = it.is_event_;
			}
		}
		tagRootStruct& operator=(const tagRootStruct& it)
		{
			if (&it == this)
			{
				return *this;
			}
			this->ns_ = it.ns_;
			this->class_ = it.class_;
			this->method_ = it.method_;
			this->router_ = it.router_;
			this->vars_ = it.vars_;
			this->structs_ = it.structs_;
            this->is_event_ = it.is_event_;
            return *this;
		}

        std::string ns_;
        std::string class_;
        std::string method_;
        std::string router_;
        std::vector<MetaVariable> vars_;
        std::unordered_map<std::string, tagMetaStruct> structs_;
        bool is_event_;
    }RootStruct;

    // Container of options that may apply to any of the source/text generators.
    struct IDLOptions
    {
        bool strict_json;
        bool skip_js_exports;
        bool output_default_scalars_in_json;
        int indent_step;
        bool output_enum_identifiers;
        bool prefixed_enums;
        bool scoped_enums;
        bool include_dependence_headers;
        bool mutable_buffer;
        bool one_file;
        bool proto_mode;
        bool generate_all;
        bool skip_unexpected_fields_in_json;
        std::string custom_ns;

        // Possible options for the more general generator below.
        enum Language
        {
            kCSharp, kMAX
        };

        Language lang;

        IDLOptions()
            : strict_json(false),
            skip_js_exports(false),
            output_default_scalars_in_json(false),
            indent_step(2),
            output_enum_identifiers(true), prefixed_enums(true), scoped_enums(false),
            include_dependence_headers(true),
            mutable_buffer(false),
            one_file(false),
            proto_mode(false),
            generate_all(false),
            skip_unexpected_fields_in_json(false),
            lang(IDLOptions::kCSharp),
            custom_ns("")
        {}
    };

    // A way to make error propagation less error prone by requiring values to be
    // checked.
    // Once you create a value of this type you must either:
    // - Call Check() on it.
    // - Copy or assign it to another value.
    // Failure to do so leads to an assert.
    // This guarantees that this as return value cannot be ignored.
    class CheckedError
    {
    public:
        explicit CheckedError(bool error)
            : is_error_(error), has_been_checked_(false)
        {}

        CheckedError &operator=(const CheckedError &other)
        {
            is_error_ = other.is_error_;
            has_been_checked_ = false;
            other.has_been_checked_ = true;
            return *this;
        }

        CheckedError(const CheckedError &other)
        {
            *this = other;  // Use assignment operator.
        }

        ~CheckedError()
        {
            assert(has_been_checked_);
        }

        bool Check()
        {
            has_been_checked_ = true; return is_error_;
        }

    private:
        bool is_error_;
        mutable bool has_been_checked_;
    };

    // Additionally, in GCC we can get these errors statically, for additional
    // assurance:
#ifdef __GNUC__
#define FLATBUFFERS_CHECKED_ERROR CheckedError \
          __attribute__((warn_unused_result))
#else
#define FLATBUFFERS_CHECKED_ERROR CheckedError
#endif

    class Parser
    {
    public:
        explicit Parser(const IDLOptions &options = IDLOptions())
            : opts(options)
        {
        }

        ~Parser()
        {
        }
        bool Parse(const char *_source,
            const char *source_filename);

    private:
        FLATBUFFERS_CHECKED_ERROR Error(const std::string &msg);
        FLATBUFFERS_CHECKED_ERROR DoParse(const char *_source,
            const char *source_filename);
        FLATBUFFERS_CHECKED_ERROR ParseRoot(std::vector<RootStruct>& rss, const char* key, json& val);
        FLATBUFFERS_CHECKED_ERROR ParseStruct(std::unordered_map<std::string, MetaStruct>& lookupt,
            const char* key, json& val);
        FLATBUFFERS_CHECKED_ERROR ParseVariable(std::unordered_map<std::string, MetaStruct>& lookupt, 
            std::vector<MetaVariable>& ret, const char* key, json& val);

    public:
        nlohmann::json json_content_;
        std::vector<RootStruct> structs_;
        std::unordered_map<std::string, MetaStruct> response_maps_;
        std::vector<RootStruct> event_structs_;
        std::string error_;         // User readable error_ if Parse() == false

        IDLOptions opts;
    };

    // Utility functions for multiple generators:

    extern std::string MakeCamel(const std::string &in, bool first = true);

    struct CommentConfig;

    extern void GenComment(const std::vector<std::string> &dc,
        std::string *code_ptr,
        const CommentConfig *config,
        const char *prefix = "");

    // Generate text (JSON) from a given FlatBuffer, and a given Parser
    // object that has been populated with the corresponding schema.
    // If ident_step is 0, no indentation will be generated. Additionally,
    // if it is less than 0, no linefeeds will be generated either.
    // See idl_gen_text.cpp.
    // strict_json adds "quotes" around field names if true.
    extern void GenerateText(const Parser &parser,
        const void *flatbuffer,
        std::string *text);
    extern bool GenerateTextFile(const Parser &parser,
        const std::string &path,
        const std::string &file_name);

    // Generate Java/C#/.. files from the definitions in the Parser object.
    // See idl_gen_general.cpp.
    extern bool GenerateGeneral(const Parser &parser,
        const std::string &path,
        const std::string &file_name);

    // Generate a make rule for the generated Java/C#/... files.
    // See idl_gen_general.cpp.
    extern std::string GeneralMakeRule(const Parser &parser,
        const std::string &path,
        const std::string &file_name);

    // Generate a make rule for the generated text (JSON) files.
    // See idl_gen_text.cpp.
    extern std::string TextMakeRule(const Parser &parser,
        const std::string &path,
        const std::string &file_names);

}  // namespace pomeloc

#endif  // FLATBUFFERS_IDL_H_

