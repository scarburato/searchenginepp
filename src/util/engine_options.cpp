#include <unistd.h>
#include <getopt.h>
#include "engine_options.hpp"

engine_options::engine_options(int argc, char **argv)
{
	static const option long_options[] = {
			/*   NAME       ARGUMENT           FLAG  SHORTNAME */
			{"top-k",	required_argument, nullptr, 'k'},
			{"run-name",	required_argument, nullptr, 'r'},
			{"algorithm",required_argument, nullptr, 'a'},
			{"batch",	no_argument,       nullptr, 'b'},
			{"data-dir",	required_argument, nullptr, 'd'},
			{"threads",	required_argument, nullptr, 't'},
			{"score",	required_argument, nullptr, 's'},
			{nullptr, 0, nullptr, 0}
	};

	int c;
	int option_index = 0;
	while ((c = getopt_long(argc, argv, "k:r:a:d:t:s:b", long_options, &option_index)) != -1)
	{
		switch (c)
		{
			case 'k':
				k = std::stoi(optarg);
				break;
			case 'r':
				run_name = optarg;
				break;
			case 'a':
				if(optarg == std::string("daat-disjunctive") or optarg == std::string("daat"))
					algorithm = DAAT_DISJUNCTIVE;
				else if(optarg == std::string("daat-conjunctive") or optarg == std::string("daat-c"))
					algorithm = DAAT_CONJUNCTIVE;
				else if(optarg == std::string("bmm"))
					algorithm = BMM;
				else
					algorithm = DAAT_DISJUNCTIVE;
				break;
			case 'b':
				batch_mode = true;
				break;
			case 'd':
				data_dir = optarg;
				break;
			case 't':
				thread_count = std::stoi(optarg);
				break;
			case 's':
				if(optarg == std::string("tfidf"))
					score = TFIDF;
				else // assume BM25
					score = BM25;
				break;
			default:
				break;
		}
	}

	if(optind < argc)
		data_dir = argv[optind];
}
