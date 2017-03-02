#include <set>
#include <map>
#include <string>
#include <cstring>
#include <vector>
#include <memory>
#include <iostream>
#include <sstream>


namespace clarg {

// generic ctor fallback
template <typename Into>
Into From(const std::string& s) {
    return Into(s);
}

// unsigned

template<> std::uint8_t From<std::uint8_t>(const std::string& s) { return stoul(s); }
template<> std::uint16_t From<std::uint16_t>(const std::string& s) { return stoul(s); }
template<> std::uint32_t From<std::uint32_t>(const std::string& s) { return stoul(s); }
template<> std::uint64_t From<std::uint64_t>(const std::string& s) { return stoull(s); }


// signed

template<> std::int8_t From<std::int8_t>(const std::string& s) { return stol(s); }
template<> std::int16_t From<std::int16_t>(const std::string& s) { return stol(s); }
template<> std::int32_t From<std::int32_t>(const std::string& s) { return stol(s); }
template<> std::int64_t From<std::int64_t>(const std::string& s) { return stoll(s); }


// floats

template<> float From<float>(const std::string& s) { return stof(s); }
template<> double From<double>(const std::string& s) { return stod(s); }
template<> long double From<long double>(const std::string& s) { return stold(s); }



enum class Type : std::uint8_t {
    NORMAL,
    DEFAULTED,
    POSITIONAL,
};

enum class Needs : std::uint8_t {
    REQUIRED,
    OPTIONAL,
};


class InputError : public std::runtime_error {
public:
    InputError(const char* msg) : std::runtime_error(msg) {}
    InputError(std::string msg) : std::runtime_error(msg.c_str()) {}
};

class ParseError : public std::runtime_error {
public:
    ParseError(const char* msg) : std::runtime_error(msg) {}
    ParseError(std::string msg) : std::runtime_error(msg.c_str()) {}
};

class UnsetArgument : public ParseError {
public:
    UnsetArgument(std::string arg) : ParseError(("required argument not given: "+arg).c_str()) {}
};


struct Descriptor {
    char short_name;
    std::string long_name;
    std::string description;
    std::string default_str;
    std::string display_str;
    Type type;
    Needs needs;

    Descriptor(
        char short_name, std::string long_name, std::string description,
        Type ty=Type::NORMAL, Needs needs=Needs::OPTIONAL,
        std::string default_str="", std::string display_str=""
    )
        : short_name(short_name)
        , long_name(long_name)
        , description(description)
        , default_str(default_str)
        , display_str(display_str)
        , type(ty)
        , needs(needs)
    {}
};


struct HelpOptions {
    std::uint16_t width{80};             // max width of the help
    std::uint8_t  indent{4};             // indent all non-leading help lines
    std::uint8_t  group_indent{4};       // separate indentation for group arguments/subgrps (not name)
    std::uint8_t  lines_between{1};      // empty lines between progline/groups/other
    std::uint8_t  lines_after_group{0};  // empty lines after the group heading
    bool          line_after_wrap{true}; // empty line after a description wraps the width
    std::string   use_prefix{"usage:"};  // prefix of the usage line

    // internal state
    std::uint16_t longest_prefix{0};       // this field is modified on each arg add to format arg-desc gap

    void print_many(std::size_t n, char c=' ', std::ostream& out=std::cout) const {
        for (std::uint8_t i = 0; i < n; ++i) { out << c; }
    }

    void print_lines(std::ostream& out=std::cout) const {
        print_many(lines_between, '\n', out);
    }

    void print_indent(std::ostream& out=std::cout) const {
        print_many(indent, ' ', out);
    }

    void print_group_indent(std::ostream& out=std::cout) const {
        print_many(indent+group_indent, ' ', out);
    }

    void wrap(std::uint16_t prefix_len, const std::string& content, std::ostream& out=std::cout) const {
        auto scribe = prefix_len;
        auto desc_it = content.begin();
        while (desc_it != content.end()) {
            bool should_preempt = false;
            if (*desc_it == ' ') { // look ahead on spaces
                // get how far we are into the content so we know where to search
                std::size_t it_dist = std::distance(content.begin(), desc_it);
                if (it_dist != std::string::npos) { // abort if EOF
                    // find the next space, but don't count yourself
                    auto offset = content.find_first_of(" ", it_dist+1);
                    // if we found one, we want to wrap early and skip the space char
                    if (offset != std::string::npos and(scribe + (offset - it_dist)) > width) {
                        should_preempt = true;
                        ++desc_it;
                    }
                }
            }

            if (should_preempt or scribe == width) {
                out << std::endl;
                print_many(prefix_len);
                scribe = prefix_len;
            }
            out << *desc_it;
            ++scribe;
            ++desc_it;
        }
    }
}; // help options



struct ParseContext {
    int num_inputs;
    const char** inputs;

    #ifndef __EXCEPTIONS
    std::vector<std::string> errors;
    #endif

    std::set<char> short_codes;
    std::set<std::string> long_codes;
    std::map<std::string, std::vector<int>> arg_to_indices; // shorts are converted to string
    std::set<int> other_indices;
};

template <typename T>
class OptionsInterface {
protected:
    T& self;
    ParseContext& parse_ctx;

public:
    std::vector<Descriptor> args;


protected:
    void validate(char s, const std::string& l) {
        if (s != 0 and (s < '!' or s > '~')) {
            std::cout << "got unprintable: " << (std::uint16_t)s << std::endl;
            auto err_str = "short names must be printable character within the non-extended ASCII set";
            #ifdef __EXCEPTIONS
                throw InputError(err_str);
            #else
                parse_ctx.errors.emplace_back(err_str);
            #endif
        }

        if (l.size() <= 1) {
            auto err_str = "long names must be more than one character";
            #ifdef __EXCEPTIONS
                throw InputError(err_str);
            #else
                parse_ctx.errors.emplace_back(err_str);
            #endif
        }
    }

    void register_codes(char c, std::string l) {
        bool inserted = false;
        if (c) {
            std::set<char>::iterator unused;
            std::tie(unused, inserted) = parse_ctx.short_codes.emplace(c);
            if (not inserted) {
                std::stringstream ss;
                ss << "duplicate short code detected: " << (char)c;
                #ifdef __EXCEPTIONS
                    throw InputError(ss.str());
                #else
                    parse_ctx.errors.push_back(ss.str());
                    return;
                #endif
            }
        }

        if (l.size()) {
            std::set<std::string>::iterator unused;
            std::tie(unused, inserted) = parse_ctx.long_codes.emplace(l);
            if (not inserted) {
                std::stringstream ss;
                ss << "duplicate long code detected: " << l;
                #ifdef __EXCEPTIONS
                    throw InputError(ss.str());
                #else
                    parse_ctx.errors.push_back(ss.str());
                    return;
                #endif
            }
        }
    }

    std::string format_short_long(const char c, const std::string& l) {
        std::stringstream ss;
        bool has_short = c != 0;
        bool has_long = l.size() != 0;
        if (has_short) { ss << "-" << c; }
        if (has_short and has_long) { ss << "/"; }
        if (has_long) { ss << "--" << l; }
        return ss.str();
    }


    
    std::pair<decltype(ParseContext::arg_to_indices)::iterator, bool> // bool marks was found
    get_arg(char s, const std::string& l, const Needs& needs) {
        bool found = false;
        auto iter = parse_ctx.arg_to_indices.find(std::string(1, s));
        if (iter == parse_ctx.arg_to_indices.end()) {
            // were we used as a long?
            iter = parse_ctx.arg_to_indices.find(l);
            if (iter == parse_ctx.arg_to_indices.end()) {
                if (needs == Needs::REQUIRED) {
                    auto err = UnsetArgument(format_short_long(s, l));
                    #ifdef __EXCEPTIONS
                        throw err;
                    #else
                        parse_ctx.errors.emplace_back(err.what());
                        return std::make_pair(iter, false);
                    #endif
                }
            } else {
                found = true;
            }
        } else {
            found = true;
        }

        return std::make_pair(iter, found);
    }

    std::pair<decltype(ParseContext::other_indices)::value_type, bool>
    get_and_pop_pos(char s, const std::string& l, std::size_t index) {
        // find the next "unclaimed" and assert it came directly after us
        auto iter = parse_ctx.other_indices.upper_bound(index);
        if (iter == parse_ctx.other_indices.end()) {
            if (*iter != (int)(index+1)) {
                std::stringstream ss;
                ss << "no argument given to " << format_short_long(s, l);
                #ifdef __EXCEPTIONS
                    throw ParseError(ss.str());
                #else
                    parse_ctx.errors.push_back(ss.str());
                    return std::make_pair(0, false);
                #endif
            }
        }

        auto result = *iter;
        parse_ctx.other_indices.erase(iter);

        return std::make_pair(result, true);
    }


public:
    OptionsInterface(T& self, ParseContext& parse_ctx)
        : self(self)
        , parse_ctx(parse_ctx)
    {
    }

    std::uint16_t calc_max_prefix(std::uint16_t indent) {
        std::uint16_t longest_prefix = 0;
        for (const Descriptor& a : args) {
            std::uint16_t len = indent + 4; //  = short val/pad

            if (a.long_name.size()) {
                len += 2 + a.long_name.size(); // "--something"
            }

            if (a.display_str.size()) {
                len += 1 + a.display_str.size(); // " SOME"
            }

            longest_prefix = std::max(longest_prefix, len);
        }

        return longest_prefix;
    }


    //
    // flag
    //

    T& flag(
            char s, std::string l, std::string d, bool& into, bool inverted=false,
            Type type=Type::NORMAL, Needs needs=Needs::OPTIONAL
    ) {
        validate(s, l);
        register_codes(s, l);
        if (inverted) {
            args.emplace_back(Descriptor(s, l, d, type, needs, "true"));
        } else {
            args.emplace_back(Descriptor(s, l, d, type, needs));
        }

        into = inverted;
        auto status = get_arg(s,l,needs);
        if (status.second) { into =  not inverted; }
        return self;
    }
    T& flag(char s, const char* l, const char* d, bool& into, bool inverted=false,
            Type type=Type::NORMAL, Needs needs=Needs::OPTIONAL
    ) {
        return flag(s, std::string(l), std::string(d), into, inverted, type, needs);
    }
    T& flag(char s, const char* d, bool& into, bool inverted=false,
            Type type=Type::NORMAL, Needs needs=Needs::OPTIONAL
    ) {
        return flag(s, std::string(""), std::string(d), into, inverted, type, needs);
    }
    T& flag(const char* l, const char* d, bool& into, bool inverted=false,
            Type type=Type::NORMAL, Needs needs=Needs::OPTIONAL
    ) {
        return flag(0, std::string(l), std::string(d), into, inverted, type, needs);
    }


    //
    // count
    //

    template <typename U>
    T& count(char s, std::string l, std::string d, U& into,
             Type type=Type::NORMAL, Needs needs=Needs::OPTIONAL
    ) {
        validate(s, l);
        register_codes(s, l);
        args.emplace_back(Descriptor(s, l, d, type, needs));

        auto iter = get_arg(s,l,needs);
        if (not iter.second) { return self; }
        into = iter.first->second.size();

        return self;
    }
    template <typename U>
    T& count(char s, const char* l, const char* d, U& into,
             Type type=Type::NORMAL, Needs needs=Needs::OPTIONAL
    ) {
        return count(s, std::string(l), std::string(d), into, type, needs);
    }
    template <typename U>
    T& count(char s, const char* d, T& into,
             Type type=Type::NORMAL, Needs needs=Needs::OPTIONAL
    ) {
        return count(s, "", d, into, type, needs);
    }
    template <typename U>
    T& count(const char* l, const char* d, U& into,
             Type type=Type::NORMAL, Needs needs=Needs::OPTIONAL
    ) {
        return count(0, std::string(l), std::string(d), into, type, needs);
    }


    //
    // arg
    //

    // NOTE: if multiple of this type are provided, the last one wins
    template <typename U>
    T& arg(char s, std::string l, std::string d, U& into,
           Type type=Type::NORMAL, Needs needs=Needs::OPTIONAL,
           std::string display=""
    ) {
        validate(s, l);
        register_codes(s, l);
        if (type == Type::DEFAULTED) {
            std::stringstream ss;
            ss << into;
            args.emplace_back(Descriptor(s, l, d, type, needs, ss.str(), display));
        }
        else {
            args.emplace_back(Descriptor(s, l, d, type, needs, "", display));
        }

        auto iter = get_arg(s,l,needs);
        if (not iter.second) { return self; }

        // pop all but the last instance of the arg
        for (std::size_t i = 0; i < (iter.first->second.size() - 1); ++i) {
            auto status = get_and_pop_pos(s,l,iter.first->second.at(i));
            // get_and_pop_pos will throw it's own exception, but if no exceptions, we need to abort here
            if (not status.second) {
                #ifndef __EXCEPTIONS
                std::stringstream ss;
                ss << "could not get positional for " << format_short_long(s, l);
                parse_ctx.errors.push_back(ss.str());
                #endif
                return self; // should only get hit in non-exception... otherwise should stack unwind
            }
        }

        auto pos = get_and_pop_pos(s,l,iter.first->second.back()).first;

        #ifdef __EXCEPTIONS
        try {
        #endif
            into = From<U>(std::string(parse_ctx.inputs[pos]));
        #ifdef __EXCEPTIONS
        } catch (const std::invalid_argument& e) {
            std::stringstream ss;
            ss << "error while parsing value of " << format_short_long(s, l) << ": " << e.what();
            throw ParseError(ss.str());
        } catch (const std::exception& e) {
            std::stringstream ss;
            ss << "error while handling " << format_short_long(s, l) << ": " << e.what();
            throw ParseError(ss.str());
        }
        #endif

        return self;
    }
    template <typename U>
    T& arg(char s, std::string l, std::string d, U& into,
           Type type=Type::NORMAL, std::string display=""
    ) {
        return arg<U>(s, l, d, into, type, Needs::OPTIONAL, display);
    }
    template <typename U>
    T& arg(char s, std::string l, std::string d, U& into, std::string display=""
    ) {
        return arg<U>(s, l, d, into, Type::NORMAL, Needs::OPTIONAL, display);
    }
    template <typename U>
    T& arg(char s, const char* l, const char* d, U& into,
           Type type=Type::NORMAL, Needs needs=Needs::OPTIONAL,
           std::string display=""
    ) {
        return arg<U>(s, std::string(l), std::string(d), into, type, needs, display);
    }
    template <typename U>
    T& arg(char s, const char* d, U& into,
           Type type=Type::NORMAL, Needs needs=Needs::OPTIONAL,
           std::string display=""
    ) {
        return arg<U>(s, "", d, into, type, needs, display);
    }
    template <typename U>
    T& arg(const char* l, const char* d, U& into,
           Type type=Type::NORMAL, Needs needs=Needs::OPTIONAL,
           std::string display=""
    ) {
        return arg<U>(0, l, d, into, type, needs, display);
    }


    //
    // arg list
    //

    template <typename U>
    T& list(char s, std::string l, std::string d, U& into,
           Type type=Type::NORMAL, Needs needs=Needs::OPTIONAL,
           std::string display=""
    ) {
        validate(s, l);
        register_codes(s, l);
        args.emplace_back(Descriptor(s, l, d, type, needs, "", display));

        auto iter = get_arg(s,l,needs);
        if (not iter.second) { return self; }

        for (auto& i : iter.first->second) {
            auto pos = get_and_pop_pos(s,l,i);
            #ifndef __EXCEPTIONS
            if (not pos.second) { return self; } // abort since we do not unwind
            #endif

            #ifdef __EXCEPTIONS
            try {
            #endif
                into.push_back(From<typename U::value_type>(std::string(parse_ctx.inputs[pos.first])));
            #ifdef __EXCEPTIONS
            } catch (const std::invalid_argument& e) {
                std::stringstream ss;
                ss << "error while parsing value of " << format_short_long(s, l) << ": " << e.what();
                throw ParseError(ss.str());
            } catch (const std::exception& e) {
                std::stringstream ss;
                ss << "error while handling " << format_short_long(s, l) << ": " << e.what();
                throw ParseError(ss.str());
            }
            #endif
        }

        return self;
    }
    template <typename U>
    T& list(char s, std::string l, std::string d, U& into,
           Type type=Type::NORMAL, std::string display=""
    ) {
        return list<U>(s, l, d, into, type, Needs::OPTIONAL, display);
    }
    template <typename U>
    T& list(char s, std::string l, std::string d, U& into, std::string display=""
    ) {
        return list<U>(s, l, d, into, Type::NORMAL, Needs::OPTIONAL, display);
    }
    template <typename U>
    T& list(char s, const char* l, const char* d, U& into,
           Type type=Type::NORMAL, Needs needs=Needs::OPTIONAL,
           std::string display=""
    ) {
        return list<U>(s, std::string(l), std::string(d), into, type, needs, display);
    }
    template <typename U>
    T& list(char s, const char* d, U& into,
           Type type=Type::NORMAL, Needs needs=Needs::OPTIONAL,
           std::string display=""
    ) {
        return list<U>(s, "", d, into, type, needs, display);
    }
    template <typename U>
    T& list(const char* l, const char* d, U& into,
           Type type=Type::NORMAL, Needs needs=Needs::OPTIONAL,
           std::string display=""
    ) {
        return list<U>(0, l, d, into, type, needs, display);
    }


    //
    // helpers
    //

    void print(std::size_t indent, const HelpOptions& fmt, std::ostream& out=std::cout) {
        for (auto& arg : args) {
            auto prefix_len = indent;

            fmt.print_many(indent, ' ', out);

            if (arg.type != Type::POSITIONAL) {
                // short name
                prefix_len += 4; // we always have to pad the short
                if (arg.short_name) {
                    out << "-" << arg.short_name << ", ";
                } else {
                    out << "    ";
                }
            }

            // long name
            if (arg.long_name.size()) {
                if (arg.type != Type::POSITIONAL) {
                    out << "--";
                    prefix_len += 2;
                }
                out << arg.long_name;
                prefix_len += arg.long_name.size();
            }

            if (arg.display_str.size()) {
                prefix_len += 1 + arg.display_str.size();
                out << " " << arg.display_str;
            }

            auto offset_len = 5 + (fmt.longest_prefix - prefix_len);
            fmt.print_many(offset_len, ' ', out);

            // arg description
            fmt.wrap(offset_len+prefix_len, arg.description, out);

            // if we have a default print it -- NOTE: always starts on a new line
            if (arg.default_str.size()) {
                out << "\n";
                fmt.print_many(offset_len+prefix_len, ' ', out);
                out << "[default: ";
                fmt.wrap(offset_len+prefix_len, arg.default_str);
                out << "]";
            }

            // if we wrapped and have request to print an extra line
            if (fmt.line_after_wrap and (arg.description.size() > (fmt.width-offset_len))) {
                out << std::endl;
            }
            out << std::endl;
        }
    }
};




class Parser; // fwdecl
class Group : public OptionsInterface<Group> {
public:
    std::string name;

    // backlink
    Parser& ctx;

    Group(const std::string& name, Parser& ctx, ParseContext& parse_ctx)
        : OptionsInterface(*this, parse_ctx)
        , name(name), ctx(ctx)
    {}

    Parser& done() { return ctx; }

    using OptionsInterface<Group>::print;
    void print(const HelpOptions& fmt, std::ostream& out=std::cout) {
        fmt.print_indent();
        out << name << ":" << std::endl;
        print(fmt.indent + fmt.group_indent, fmt, out);
    }
}; // Group class





class Parser : public OptionsInterface<Parser>{
protected:
    // store inputs
    ParseContext ctx;

    // basic info
    std::string progname;
    std::string description;
    std::string long_description;
    std::string footer_text;
    HelpOptions help_opts;

    // parsing options
    std::string terminator; // this ends all flag parsing, presumes remainder is positional (--)

    // other argument types
    OptionsInterface<Parser> positionals;
    std::vector<Group> groups;


    bool is_short(const char* arg) {
        auto len = strlen(arg); // TODO: strlen is bad
        if (len < 2) {
            return false;
        }

        return arg[0] == '-' and arg[1] != '-';
    }

    bool is_long(const char* arg) {
        auto len = strlen(arg); // TODO: strlen is bad
        if (len < 3) {
            return false;
        }

        return strncmp(arg, "--", 2) == 0;
    }

    // separate all the required and optional flags
    void partition_args(
        const std::vector<Descriptor>& desc,
        std::vector<std::string>& opt_short,
        std::vector<std::string>& opt_long,
        std::vector<std::string>& req_opt
    ) {
        for (auto& a : desc) {
            switch (a.needs) {
            case Needs::OPTIONAL:
                if (a.short_name != 0) { opt_short.push_back(std::string(1, a.short_name)); }
                else if (a.long_name.size() != 0) { opt_long.push_back(a.long_name); }
                break;
            case Needs::REQUIRED:
                if (a.short_name != 0) { req_opt.push_back(std::string(1, a.short_name)); }
                else if (a.long_name.size() != 0) { req_opt.push_back(a.long_name); }
                break;
            }
        }
    }



public:
    //! Does not supply a program name, so simply uses argv[0].
    Parser(const std::string& description) : Parser("", description) {}

    //! Create a parser using the given program name and description.
    Parser(const std::string& progname, const std::string& description)
        : OptionsInterface<Parser>(*this, ctx)
        , ctx({
            0, nullptr,
            #ifndef __EXCEPTIONS
            {}, // errors vec
            #endif
            {},{},{},{}}
        )
        , progname(progname)
        , description(description)
        , terminator("--")
        , positionals(*this, ctx)
    {}


    //! This function should be called before _any_ arguments are specified.
    Parser& from(const int argc, const char** argv) {
        ctx.num_inputs = argc;
        ctx.inputs = argv;
        bool terminator_hit = false;

        for (int i = 1; i < argc; ++i) {
            const char* str = argv[i];
            auto len = strlen(str); // TODO: :( strlen
            if (len == 0) { continue; } // TODO: what to do?


            // check if it's the terminator and if so, continue
            if (len == terminator.size() and strncmp(str, terminator.c_str(), len) == 0) {
                terminator_hit = true;
                continue;
            }

            // if we've hit the terminator, go directly to the misc pile
            if (terminator_hit) {
                ctx.other_indices.insert(i);
                continue;
            }

            // parse as short (or run of shorts)
            if (is_short(str)) {
                for (std::size_t k = 1; k < len; ++k) {
                    std::string key(1, str[k]); 
                    auto iter = ctx.arg_to_indices.find(key);
                    if (iter == ctx.arg_to_indices.end()) {
                        ctx.arg_to_indices.emplace(key, std::vector<int>{i});
                    } else {
                        ctx.arg_to_indices[key].push_back(i);
                    }
                }
                continue;
            }

            // parse as long
            if (is_long(str)) {
                std::string key(str + 2, len-2);
                if (i == (argc-1)) { // do we have another
                    std::stringstream ss;
                    ss << "no argument given to " << key;
                    #ifdef __EXCEPTIONS
                        throw ParseError(ss.str());
                    #else
                        ctx.errors.push_back(ss.str());
                        return *this;
                    #endif
                }

                auto iter = ctx.arg_to_indices.find(key);
                if (iter == ctx.arg_to_indices.end()) {
                    ctx.arg_to_indices.emplace(key, std::vector<int>{i});
                } else {
                    iter->second.push_back(i);
                }
                continue;
            }

            // positional
            ctx.other_indices.emplace(i);
        }

        return *this;
    }

    #ifndef __EXCEPTIONS
    //! Get all the errors encountered while parsing arguments (and their descriptors).
    std::vector<std::string>& errors() {
        return ctx.errors;
    }
    #endif

    //! Set the program name printed in the help dialog.
    void prog(const std::string& name) { progname = name; }

    //! Set the flag terminator which forces all further arguments to be parsed as positionals.
    void flag_terminator(const std::string& term) { terminator = term; }

    //! Get the help formatting options for modification.
    HelpOptions& help_options() { return help_opts; }

    //! Set a long description for after the progline but before the arguments.
    Parser& header(std::string content) { long_description = content; return *this; }

    //! Set a footer for after the arguments.
    Parser& footer(std::string content) { footer_text = content; return *this; }


    Group& group(std::string name) {
        groups.emplace_back(name, *this, ctx);
        return groups.back();
    }



    template <typename T>
    Parser& pos(std::string l, std::string d, T& into) {
        positionals.args.emplace_back(Descriptor(0, l, d, Type::POSITIONAL));

        if (parse_ctx.other_indices.empty()) {
            std::stringstream ss;
            ss << "expected a positional argument for: " << l;
            #ifdef __EXCEPTIONS
                throw ParseError(ss.str());
            #else
                ctx.errors.push_back(ss.str());
                return *this;
            #endif
        }

        auto iter = parse_ctx.other_indices.begin();
        #ifdef __EXCEPTIONS
        try {
        #endif
            into = From<T>(std::string(parse_ctx.inputs[*iter]));
        #ifdef __EXCEPTIONS
        } catch (const std::invalid_argument& e) {
            std::stringstream ss;
            ss << "error while parsing value of " << l << ": " << e.what();
            throw ParseError(ss.str());
        } catch (const std::exception& e) {
            std::stringstream ss;
            ss << "error while handling " << l << ": " << e.what();
            throw ParseError(ss.str());
        }
        #endif

        parse_ctx.other_indices.erase(iter);
        return *this;
    }
    template <typename T>
    Parser& pos(const char* l, const char* d) {
        return pos<T>(std::string(l), std::string(d));
    }

    /** Number of "unclaimed" positional arguments.
     * This will return greater than 0 if Parser::gather is not called.
     * The application should handle this as needed (or just call gather).
     */
    std::size_t unclaimed() { return parse_ctx.other_indices.size(); }



    template <typename T>
    Parser& gather(T& into) {
        for (auto& iter : parse_ctx.other_indices) {
            #ifdef __EXCEPTIONS
            try {
            #endif
                into.push_back(From<typename T::value_type>(std::string(parse_ctx.inputs[iter])));
            #ifdef __EXCEPTIONS
            } catch (const std::invalid_argument& e) {
                std::stringstream ss;
                ss << "error while parsing value of unnamed positional: " << e.what();
                throw ParseError(ss.str());
            } catch (const std::exception& e) {
                std::stringstream ss;
                ss << "error while handling unnamed positional: " << e.what();
                throw ParseError(ss.str());
            }
            #endif

            parse_ctx.other_indices.erase(iter);
        }
        return *this;
    }


    using OptionsInterface<Parser>::print;
    //! Print the help dialog.
    void print(std::ostream& out=std::cout) {
        std::vector<std::string> opt_short;
        std::vector<std::string> opt_long;
        std::vector<std::string> req_opt;

        // partition the main args
        partition_args(args, opt_short, opt_long, req_opt);

        // find the longest argument length
        // lets use align all the descriptions
        auto grp_depth = help_opts.indent + help_opts.group_indent;
        help_opts.longest_prefix = std::max(
            calc_max_prefix(help_opts.indent),
            positionals.calc_max_prefix(grp_depth)
        );
        for (auto& g : groups) {
            partition_args(g.args, opt_short, opt_long, req_opt);
            help_opts.longest_prefix = std::max(help_opts.longest_prefix, g.calc_max_prefix(grp_depth));
        }


        out << progname << " - "  << description << std::endl;
        help_opts.print_lines();

        std::stringstream usage;
        auto usage_offset = help_opts.use_prefix.size() + 2 /*spaces*/ + progname.size();
        usage << help_opts.use_prefix << " " << progname << " ";
        if (opt_short.size()) { 
            usage << "[-";
            for (auto& o : opt_short) {
                usage << o;
            }
            usage << "]";
        }
        if (opt_short.size() and (opt_long.size() or req_opt.size())) { usage << " "; }
        if (opt_long.size()) {
            for (auto& o : opt_long) {
                usage << " [--" << o << "]";
            }
        }
        if (req_opt.size()) {
            for (auto& o : req_opt) {
                usage << " [" << (o.size() == 1 ? "-" : "--") << o << "]";
            }
        }
        for (auto& p : positionals.args) {
            usage << " " << p.long_name;
        }
        help_opts.wrap(usage_offset, usage.str()); out << std::endl;
        
        help_opts.print_lines();

        if (long_description.size()) {
            help_opts.wrap(0, long_description);
            out << std::endl; // newline after content
            help_opts.print_lines();
        }

        print(help_opts.indent, help_opts, out);
        for (auto& g : groups) {
            help_opts.print_lines();
            g.print(help_opts, out);
        }

        help_opts.print_lines();
        help_opts.print_indent();
        out << "positionals: " << std::endl;
        positionals.print(help_opts.indent, help_opts, out);

        if (footer_text.size()) {
            help_opts.print_lines();
            help_opts.wrap(0, footer_text);
        }
    }

    friend std::ostream& operator<<(std::ostream& s, Parser& p) {
        p.print(s);
        return s;
    }
}; // Parser class


} // namespace
