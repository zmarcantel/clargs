#include <cassert>
#include "clargs.hpp"



struct CustomType {
    std::vector<std::string> segments;
    CustomType(const std::string& in) {
        std::stringstream ss(in);
        std::string item;
        std::vector<std::string> tokens;
        while (getline(ss, item, ':')) {
            segments.push_back(item);
        }
    }

    friend std::ostream& operator<<(std::ostream& s, const CustomType& ven) {
        for (std::size_t i = 0; i < ven.segments.size(); ++i) {
            s << ven.segments.at(i) << ((i < (ven.segments.size()-1)) ? ":" : "");
        }
        return s;
    }
};



struct Options {
    bool need_help{false};
    std::uint8_t verbosity{0};

    std::string output{"a.out"};

    std::uint8_t max_phys{0};
    double word_size{64};
    bool word_aligned{false};

    bool warn{false};
    std::vector<std::string> warnings;
    std::uint8_t bus{1};

    CustomType vendor{{}};
    std::string subcommand{"compile"};
    std::vector<std::string> positionals;

    friend std::ostream& operator<<(std::ostream& s, const Options& opts) {
        s << "output: " << opts.output << "\n"
          << "command: " << opts.subcommand << "\n"
          << "verbosity: " << (std::size_t)opts.verbosity << "\n"

          << "max address: " << (std::uint16_t)opts.max_phys << " MiB\n"
          << "word size: " << opts.word_size << " bits\n"
          << "word aligned: " << (opts.word_aligned ? "y" : "n") << "\n"

          << "warnings enabled: " << (opts.warn ? 'y' : 'n') << "\n";
        if (opts.warn) {
            s << "warnings:\n";
            for (auto& w : opts.warnings) {
                s << "\t" << w << "\n";
            }
        }
        s << "bus ID: " << (std::uint16_t)opts.bus << "\n"
        ;

        if (opts.positionals.size()) { s << "Positionals:\n"; }
        for (auto& p : opts.positionals) {
            s << "\t" << p << "\n";
        }

        return s;
    }
};


int main(const int argc, const char** argv) {
    Options opts;

    clarg::Parser args("testing", "just a simple testing app");
    args.from(argc, argv)
        .header("Lorem Ipsum is simply dummy text of the printing and typesetting industry. Lorem Ipsum has been the industry's standard dummy text ever since the 1500s, when an unknown printer took a galley of type and scrambled it to make a type specimen book. It has survived not only five centuries, but also the leap into electronic typesetting, remaining essentially unchanged. It was popularised in the 1960s with the release of Letraset sheets containing Lorem Ipsum passages, and more recently with desktop publishing software like Aldus PageMaker including versions of Lorem Ipsum.")
        .footer("created by: Zach Marcantel <zmarcantel@gmail.com>\n")
        .flag('h', "help", "print this dialog", opts.need_help)
        .count('v', "verbose", "increase the verbosity of the program", opts.verbosity)
        .arg<std::string>('o', "output", "output path for the resulting object/binary",
                          opts.output, clarg::Type::DEFAULTED, "FILE")
        .flag('w', "warn-all", "toggle all warnings", opts.warn)
        .list<std::vector<std::string>>('W', "warn", "toggle a specific warning", opts.warnings)
        .group("architecture")
            .arg<std::uint8_t>('m', "max-phys", "max hardware memory address", opts.max_phys, "MiB")
            .arg<double>("word-size", "number of bits in the maximum word size",opts.word_size)
            .flag("word-aligned", "all memory operations and instructions must be word aligned",
                  opts.word_aligned)
            .done()
        .group("outputs")
            .arg<std::uint8_t>('s', "sound-bus", "ID of the sound bus", opts.bus)
            .done()
        .arg<CustomType>("vendor-id", "a colon-separated tuple of vendor information", opts.vendor)
        .pos<std::string>("subcommand", "first positional is a subcommand", opts.subcommand)
        .gather<std::vector<std::string>>(opts.positionals)
    ;


    if (opts.need_help) { args.print(); }

    assert(opts.need_help == true);
    assert(opts.verbosity == 7);

    assert(opts.output == "fuck.yeah");

    assert(opts.max_phys == 100);
    assert(opts.word_size == 128);
    assert(opts.word_aligned == true);

    assert(opts.warn == true);
    assert(opts.warnings.size() == 3);
    assert(opts.warnings.at(0) == "all");
    assert(opts.warnings.at(1) == "abi");
    assert(opts.warnings.at(2) == "inline");
    assert(opts.bus == 9);

    std::vector<std::string> expect_vendor{"abcd","123","xyz"};
    assert(opts.vendor.segments == expect_vendor);
    assert(opts.subcommand == "parse");
    assert(opts.positionals.size() == 3);
    assert(opts.positionals.at(0) == "one");
    assert(opts.positionals.at(1) == "two");
    assert(opts.positionals.at(2) == "three");

    #ifndef __EXCEPTIONS
    assert(args.errors().size() == 0);
    #endif

    std::cout << "\n\n\nParsed:\n" << opts;

    return 0;
}
