clargs
======

Simple, lightweight, and full-featured argument parsing library that does not require `RTTI`.

To satisfy the `-fno-rtti` _requirement_, `clargs` does some things differently.


differences
-----------

#### parse first, ask later

It is a requirement of `clargs` that the `Parser::from(argc, argv)` function be called _before_ any arguments
are registered. This is because the `(argc, argv)` pair _must_ be parsed first.

The rationale is simple. Given a list of indices into `argv`, many unique/novel/helpful argument types
can be constructed with little to no processing. Take the following for instance:

```cpp
int main(int argc, char** argv) {
    std::size_t verbosity = 0;
    std::string subcommand("commit");
    std::vector<std::string> files;

    args::Parser parse("test");
    parse.from(argc, argv)
         .count("v", "verbose", "increase the verbosity", verbosity)
         .pos<std::string>("subcommand", subcommand)
         .gather<std::vector<std::string>>("files", files);

    return 0;
}
```

`Parser::count` can be implemented simply by asking the size of the indices list. We already had to `O(n)` scan
`argv` in `Parser::from` but now we can skip any other processing.

`Parser::pos` adds a required positional argument. Each call to `Parser::pos` consumes the first positional in
the list, forcing it to exist and asserting it is parseable. This also requires no processing other than running
the already-processed index through the `args::From<T>(...)` filter (falling back to constructor if needed).

`Parser::gather` works very much the same as `Parser::pos` except it takes all "unclaimed" positional arguments
and constructs/places them into the provided container.

Functionality that would be non-trivial to implement with pure string scanning easily falls out of this strategy.
A single `O(n)` scan of `argv` and `O(log N)` lookups (where `N` is the number of _remaining_ argument descriptions,
decreasing every time an descriptor is parsed) for argument indices means we can ask questions about inputs very quickly
with minimal string operations.




#### no RTTI


Rather than parsing the `(argc, argv)` pair last and storing a convoluted class hierarchy and using `dynamic_cast<T>(...)`,
the pair must be consumed _first_ for the reasons stated above.

To facilitate this, the following API is used:

```cpp
struct ColonSepList {
    std::vector<std::string> segments;
    ColonSepList(const std::string& in) {
        std::stringstream ss(in);
        std::string item;
        std::vector<std::string> tokens;
        while (getline(ss, item, ':')) {
            segments.push_back(item);
        }
    }
};

int main(int argc, char** argv) {
    ColonSepList list;
    double my_double = 5.0;
    std::vector<std::uint8_t>> id_list;

    args::Parser parse("test");
    parse.from(argc, argv)
         .arg<ColonSepList>("l", "list", "a colon separated list of things", list)
         .arg<double>("d", "double", "takes an argument and implicitly uses stod() to convert", my_double;
         .list<std::vector<std::uint8_t>>("i", "id", "inserts every instance into the container using T::push_back",
                                          id_list);

    return 0;
}
```


The string-to-`T` conversion is done with the `args::From<T>(const std::string&)` functions.
Numerics are supported out of the box, but any type (`ColonSepList` for instance) can be used assuming the
type has a constructor of the form: `T(const std::string&)`.

This approach allows `RAII` defaulting as well as argument parsing... consider the following:

```cpp
// NOTE: this is purely an example -- please do not base code off of this class
//       as not all errors are handled appropriately. for demonstration only.
class MappedFile {
protected:
    std::size_t fsize;
    int fd;
    char* contents;

public:
    MappedFile() = delete;
    MappedFile(const std::string& name) : MappedFile(name.c_str()) {}
    MappedFile(const char* name)
        : fsize(0)
        , fd(open(name, O_RDONLY))
    {
        if (fd == -1) {
            std::stringstream ss;
            ss << "failed to open " << name << ", errno=" << (std::size_t)errno;
            throw std::runtime_error(ss.str());
        }

        struct stat sb;
        fstat(fd, &sb);
        fsize = sb.st_size;
        contents = (char*)mmap(nullptr, fsize, PROT_READ, MAP_SHARED, fd, 0);

        if (contents == MAP_FAILED) {
            std::stringstream ss;
            ss << "failed to mmap " << name << ", errno=" << (std::size_t)errno;
            throw std::runtime_error(ss.str());
        }
    }

    ~MappedFile() {
        if (fd == -1) { return; }

        // do not throw here... some safety issues, but hopefully kernel helps us out
        munmap(contents, fsize);
        close(fd);

        fname = nullptr;
        fd = -1;
        fsize = 0;
    }
};



int main(int argc, char** argv) {
    std::string subcommand{"compile"};
    std::vector<MappedFile> files;

    args::Parser parse("test");
    parse.from(argc, argv)
         .pos<std::string>("subcommand", subcommand)
         .gather<decltype(files)>("files", files);

    return 0;
}
```

The above example not only parses the file list, but `open()`s and `mmap()`s the files in a manner that
is compile-time checked and requires no `RTTI`.




supported argument types
------------------------

#### terminator

While not an argument, we support setting the terminator that assumes all remaining args are positionals.
This defaults to `--`.

```cpp
int main(int argc, char** argv) {
    bool print_help = false;
    std::string pos;

    args::Parser parse("test", "prog-line description");
    parse.flag_terminator("++"); // use ++ as the terminator

    parse.from(argc, argv)
         .flag('h', "help", "print the help dialog", print_help)
         .pos("positional", pos);
         
```



#### flag

A simple boolean.

```cpp
int main(int argc, char** argv) {
    bool print_help = false;
    bool required_arg = false; // args::ParseError thrown if not provided
    bool inverted_arg = true; // defaults true, if provided on CLI, set to false

    args::Parser parse("test");
    parse.from(argc, argv)
         .flag('h', "help", "print the help dialog", print_help)
         .flag("required", "a required argument", required_arg, false, args::Type::NORMAL, args::Needs::REQUIRED)
         .flag('i', "an inverted and defaulted arg", required_arg, true, args::Type::DEFAULTED, args::Needs::REQUIRED);

    return 0;
}
```


#### count

Counts the number of times the argument was given.

```cpp
int main(int argc, char** argv) {
    std::size_t verbosity = 0;
    std::size_t offset = 5;

    args::Parser parse("test");
    parse.from(argc, argv)
         .count('v', "verbose", "increment verbosity", verbosity)
         .count('o', "offset",  "increments the offset", offset, args::Type::DEFAULTED);

    return 0;
}
```


#### arg

Simple `-s val --long other` parsing.

If multiple are provided, only the last is used.

```cpp
int main(int argc, char** argv) {
    std::uint64_t value = 0;
    MyCustomType  other; // NOTE: must have a ctor of signature `MyCustomType(const std::string&)`

    args::Parser parse("test");
    parse.from(argc, argv)
         .arg('v', "value", "set the value", value, args::Type::NORMAL, args::Needs::REQUIRED)
         .arg('o', "other", "sets the other value", other, args::Type::DEFAULTED);

    return 0;
}
```


#### list

Similar to `count(...)`, but saves the argument values in the given container using
`T::push_back(args::Fromt<T(..))` semantics.

```cpp
int main(int argc, char** argv) {
    std::vector<std::uint64_t> offsets;

    args::Parser parse("test");
    parse.from(argc, argv)
         .list('o', "offset", "add an offset to the list", offsets);

    return 0;
}
```


#### positionals

An argument that does not have rules associated with it.

These can be taken in order using `pos<T>(...)` or wholesale using `gather<T>(...)`.

__NOTE:__ positionals __must__ follow all other argument times. Similarly, you cannot `gather` before a `pos`.

```cpp
int main(int argc, char** argv) {
    std::string subcommand{"least-common-denom"};
    std::vector<int> nums;

    args::Parser parse("test");
    parse.from(argc, argv)
         .pos("subcommand", subcommand) // parse the first positional as a string into `subcommand`
         .gather("numbers", nums);      // construct all others as `int` in the `nums` container using `From<int>(...)`

    return 0;
}
```




help formatting
---------------

Formatting your `-h/--help` (though, these flags are entirely up to the user) is a common pain point.

`clargs` tries to be as flexible as possible in the formatting of this menu.


#### printing the help

To print the help menu, simply call `Parser::print(std::ostream&)` (which defaults to `std::cout`) or use
`operator<<`.

#### groups

Grouping arguments is a common paradigm also supported by `clargs`.

Here is a brief example of usage:

```cpp
int main(int argc, char** argv) {
    int something = 0;
    std::string other("other");
    std::string command("blah");

    args::Parser parse("test");
    parse.from(argc, argv)
         .group("random")
            .arg('s', "some", "some argument", something)
            .arg('o', "other", "some other argument", other)
            .done()
         .pos("command", command);

    return 0;
}

```



#### formatting the help

The following options are able to be set on the `HelpOptions` struct on the `Parser` instance. You can get a
mutable reference with `Parser::help_options()`.


1. width
    * maximum width of the help dialog
    * all text is wrapped at this length
2. indent
    * number of spaces that the argument list is offset by
3. group_indent
    * number of spaces a group's argument list is offset by
    * this is added to the `indent` value
4. lines_between
    * number of lines between the progline, usage line, description, arg list, etc.
    * anywhere there are empty lines to delineate information, this many empty lines are used
5. lines_after_group
    * by default, no empty lines are inserted between consecutive groups
    * this number of lines separate the end of a group's arg list and the next group's heading.
6. line_after_wrap
    * default `false`
    * if `true`, if the arg description wraps the width an empty line is inserted after it
7. use_prefix
    * string to use as the "usage" header [defaults to `usage:`]



#### additional information

The following additional information can be added to the help dialog.

1. header
    * a long description/preamble of the program displayed under the usage line but before the arg list
    * `Parser::header(std::string)`
2. footer
    * a summation that is printed after the arg list
    * generally used for copyright/author information
    * `Parser::footer(std::string)`
