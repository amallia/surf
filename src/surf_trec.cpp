#include <unistd.h>
#include <stdlib.h>
#include <iostream>
#include <iomanip>
#include <ctime>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "surf/query.hpp"
#include "sdsl/config.hpp"
#include "surf/indexes.hpp"
#include "surf/query_parser.hpp"

typedef struct cmdargs {
    std::string collection_dir;
    std::string query_file;
    std::string output_file;
    uint64_t k;
} cmdargs_t;

void
print_usage(char* program)
{
    fprintf(stdout,"%s -c <collection directory> -q <query file> -k <top-k> -o <output.csv>\n",program);
    fprintf(stdout,"where\n");
    fprintf(stdout,"  -c <collection directory>  : the directory the collection is stored.\n");
    fprintf(stdout,"  -q <query file>  : the queries to be performed.\n");
    fprintf(stdout,"  -k <top-k>  : the top-k documents to be retrieved for each query.\n");
    fprintf(stdout,"  -o <output.csv>  : output results to file in csv format.\n");
};

cmdargs_t
parse_args(int argc,char* const argv[])
{
    cmdargs_t args;
    int op;
    args.collection_dir = "";
    args.query_file = "";
    args.output_file = "";
    args.k = 10;
    while ((op=getopt(argc,argv,"c:q:k:o:")) != -1) {
        switch (op) {
            case 'c':
                args.collection_dir = optarg;
                break;
            case 'q':
                args.query_file = optarg;
                break;
            case 'o':
                args.output_file = optarg;
                break;
            case 'k':
                args.k = std::strtoul(optarg,NULL,10);
                break;
            case '?':
            default:
                print_usage(argv[0]);
        }
    }
    if (args.collection_dir==""||args.query_file=="") {
        std::cerr << "Missing command line parameters.\n";
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }
    return args;
}

int main(int argc,char* const argv[])
{
    using clock = std::chrono::high_resolution_clock;
    /* parse command line */
    cmdargs_t args = parse_args(argc,argv);

    /* parse repo */
    auto cc = surf::parse_collection(args.collection_dir);

    /* parse queries */
    std::cout << "Parsing query file '" << args.query_file << "'" << std::endl;
    auto queries = surf::query_parser::parse_queries(args.collection_dir,args.query_file);
    std::cout << "Found " << queries.size() << " queries." << std::endl;

    /* define types */
    using surf_index_t = INDEX_TYPE;
    std::string index_name = IDXNAME;

    /* load the index */
    surf_index_t index;
    auto load_start = clock::now();
    construct(index, "", cc, 0);
    index.load(cc);
    auto load_stop = clock::now();
    auto load_time_sec = std::chrono::duration_cast<std::chrono::seconds>(load_stop-load_start);
    std::cout << "Index loaded in " << load_time_sec.count() << " seconds." << std::endl;

    /* process the queries */
    std::map<uint64_t,surf::result_t> query_results;

    for(const auto& query: queries) {
        auto id = std::get<0>(query);
        auto qry_tokens = std::get<1>(query);
        std::cout << "[" << id << "] |Q|=" << qry_tokens.size(); std::cout.flush();

        // run the query
        auto qry_start = clock::now();
        auto results = index.search(qry_tokens,args.k);
        auto qry_stop = clock::now();

        auto query_time = std::chrono::duration_cast<std::chrono::microseconds>(qry_stop-qry_start);
        std::cout << " TIME = " << std::setprecision(5)
                  << query_time.count() / 1000.0 
                  << " ms" << std::endl;

        query_results[id] = results;
    }
    /* output results to csv */
    std::string output_file = args.output_file;
    if(output_file.empty()) {
        char time_buffer [80] = {0};
        std::time_t t = std::time(NULL);
        auto timeinfo = localtime (&t);
        strftime (time_buffer,80,"%F-%H:%M:%S",timeinfo);
        output_file = "surf-timings-" + index_name + "-k" + std::to_string(args.k) 
                       + "-" + std::string(time_buffer) + ".trec";
    }
    std::cout << "Writing timing results to '" << output_file << "'" << std::endl;

    /* output */
    {
    	/* load the url mapping */

        std::ofstream resfs(output_file);
        if(resfs.is_open()) {
            for(const auto& res: query_results) {
                for (size_t j=0; j<output_k; j++) {
                    of << res.first << "\t"
                       << "Q0" << "\t"
                       << index.docmap.name(std::get<0>(res.list[j])) << "\t"
                       << j                                           << "\t"
                       << std::get<1>(res.list[j])                    << "\t"
                       << index_name                          << std::endl;
                }
            }
        } else {
            perror("could not output results to file.");
        }
    }


    return EXIT_SUCCESS;
}
