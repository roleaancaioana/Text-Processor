#include <mpi.h>
#include <fstream>
#include <unistd.h>
#include <bits/stdc++.h>

#define NUM_THREADS 4

using namespace std;

struct struct_thread_master {
	char* fileName;
	int id;
};

struct struct_thread_worker {
	int rank;
	int id;
	int nrLines;
	int maxThreads;
};

vector<string> current_paragraph;
vector<string> paragraphs[NUM_THREADS];
vector<string> genre_in_order;

int modified_lines;

// Credits: https://www.tutorialspoint.com/tokenizing-a-string-in-cplusplus
vector<string> tokenize_string(string my_string, char delim) {
	stringstream ss(my_string);
	string temp_str;
	vector<string> tokens;

	while (getline(ss, temp_str, delim)) {
		tokens.push_back(temp_str);
	}

	return tokens;
}

string horror(string my_paragraph) {
	string tmp;
	for (int j = 0; j < my_paragraph.size(); ++j) {
		if (isalpha(my_paragraph[j]) && my_paragraph[j] != ' ' && my_paragraph[j] != 'a' && my_paragraph[j] != 'e' && my_paragraph[j] != 'i' &&
			my_paragraph[j] != 'o' && my_paragraph[j] != 'u' && my_paragraph[j] != 'A' && my_paragraph[j] != 'E' &&
			my_paragraph[j] != 'I' && my_paragraph[j] != 'O' && my_paragraph[j] != 'U') {
			tmp += my_paragraph[j];

			if (my_paragraph[j] < 'A' || my_paragraph[j] > 'Z') {
				tmp += my_paragraph[j];
			} else {
				tmp += tolower(my_paragraph[j]); 
			}
		} else {
			tmp += my_paragraph[j];
		}
	}
	return tmp;
}

void comedy(string& my_paragraph) {
	int pos;
	int n = my_paragraph.size();

	for (int j = 0; j < n; ++j) {
		if (my_paragraph[j] != ' ') {
			pos = 0;
			while (my_paragraph[j] != ' ') {
				if (j >= n) {
					break;
				}
				if (pos % 2 == 1) {
					my_paragraph[j] = toupper(my_paragraph[j]);
				}
				pos++;
				j++;
			}
		}
	}
}

string science_fiction(string my_paragraph) {
	string tmp;
	vector<string> split_string = tokenize_string(my_paragraph, ' ');
	int n = split_string.size();

	for (int i = 0; i < n; ++i) {
		if (i >= 6) {
			if ((i + 1) % 7 == 0) {
				reverse(split_string[i].begin(), split_string[i].end());
			}
		}

		tmp += split_string[i] + " ";
	}
	return tmp;
}

pthread_barrier_t threads_worker_barrier;
pthread_barrier_t barrier_threads_processors;

void* thread_master_function(void *arg) {
	struct struct_thread_master st = *(struct struct_thread_master*) arg;
	string nameOfFile = string(st.fileName);

	ifstream myFile(nameOfFile);

	queue<string> sentParagraphs;
	string type;

	if (st.id == 0) {
    	type = "horror";
    } else if (st.id == 1) {
    	type = "comedy";
    } else if (st.id == 2) {
    	type = "fantasy";
    } else if (st.id == 3) {
    	type = "science-fiction";
    }
    string line;

	while (getline(myFile, line)) {
		if (line == "horror" || line == "comedy" || line == "fantasy" || line == "science-fiction") {
			if (st.id == 0) {
				genre_in_order.push_back(line);
			}
			string text;
			if (line == type) {
				while (getline(myFile, line) && line != "") {
					text += line;
					text += '\n';
				}
				sentParagraphs.push(text);
			}
		}
	}

	// Trebuie sa stim cand sa ne oprim din a mai trimite paragrafe workerilor
	int size_sentParagraphs = sentParagraphs.size();
	MPI_Send(&size_sentParagraphs, 1, MPI_INT, st.id + 1, 0, MPI_COMM_WORLD);

	while (!sentParagraphs.empty()) {
		string firstParagraph = sentParagraphs.front();
		int size = firstParagraph.size();

		MPI_Send(&size, 1, MPI_INT, st.id + 1, 0, MPI_COMM_WORLD);
		MPI_Send(firstParagraph.c_str(), size, MPI_CHAR, st.id + 1, 0, MPI_COMM_WORLD);

		int received_paragraph_length;
		MPI_Recv(&received_paragraph_length, 1, MPI_INT, st.id + 1, 0, MPI_COMM_WORLD, NULL);

		char aux_received_paragraph[received_paragraph_length];
		MPI_Recv(aux_received_paragraph, received_paragraph_length, MPI_CHAR, st.id + 1, 0, MPI_COMM_WORLD, NULL);

		string received_paragraph = string(aux_received_paragraph, received_paragraph_length);

		paragraphs[st.id].push_back(received_paragraph);
		sentParagraphs.pop();
	}
	
	pthread_barrier_wait(&threads_worker_barrier);

	if (st.id == 0) {
		int counter_paragraph[4] = {0};
		string output_file_name = nameOfFile.substr(0, nameOfFile.size() - 4);
		output_file_name += ".out";
		ofstream my_out(output_file_name);

		for (int i = 0; i < genre_in_order.size(); ++i) {
			int index;
			my_out << genre_in_order[i] << '\n';

			if (genre_in_order[i] == "horror") {
				index = 0;
			} else if (genre_in_order[i] == "comedy") {
				index = 1;
			} else if (genre_in_order[i] == "fantasy") {
				index = 2;
			} else if (genre_in_order[i] == "science-fiction") {
				index = 3;
			}

			my_out << paragraphs[index][counter_paragraph[index]] << '\n';
			counter_paragraph[index]++;
		}
	}
	pthread_exit(NULL);
}

void *thread_worker_processor(void *arg) {
	struct struct_thread_worker st = *(struct struct_thread_worker*) arg;
	
	while (modified_lines < st.nrLines) {
		int start_line = st.id * 20 + modified_lines;
		int end_line = start_line + 20;

		if (end_line > st.nrLines) {
			end_line = st.nrLines;
		}

		if (start_line < st.nrLines) {
			for (int i = start_line; i < end_line; ++i) {
				if (st.rank == 1) {					
					current_paragraph[i] = horror(current_paragraph[i]);
				}

				if (st.rank == 2) {
					comedy(current_paragraph[i]);
				} 

				if (st.rank == 3) {
					current_paragraph[i][0] = toupper(current_paragraph[i][0]);

					for (int j = 0; j < current_paragraph[i].size() - 1; ++j) {
						if (current_paragraph[i][j] == ' ' && current_paragraph[i][j + 1] >= 'a' && current_paragraph[i][j + 1] <= 'z') {
							current_paragraph[i][j + 1] = toupper(current_paragraph[i][j + 1]);
						}
					}
				}

				if (st.rank == 4) {
					current_paragraph[i] = science_fiction(current_paragraph[i]);
				} 
			}
		}

		// Toate thread-urile trebuie sa astepte terminarea procesarii paragrafului
		pthread_barrier_wait(&barrier_threads_processors);
		if (st.id == 0) {
			modified_lines += 20 * st.maxThreads;
		}
		// Toate thread-urile trebuie sa astepte ca modified_lines sa fie actualizat
		pthread_barrier_wait(&barrier_threads_processors);
	}

	pthread_barrier_wait(&barrier_threads_processors);

	if (st.id == 0) {
		string processed_paragraph;
		for (string s : current_paragraph) {
			processed_paragraph += s;
			processed_paragraph += "\n";
		}

		modified_lines = 0;
		int size = processed_paragraph.size();
		MPI_Send(&size, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
		MPI_Send(processed_paragraph.c_str(), size, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
	}
	pthread_exit(NULL);
}

void *thread_worker_receiver(void *arg) {
	struct struct_thread_worker st = *(struct struct_thread_worker*) arg;

	int number_of_paragraphs;
	MPI_Recv(&number_of_paragraphs, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, NULL);

	for (int i = 0; i < number_of_paragraphs; i++) {
		int P;
		int size_paragraph;
		MPI_Recv(&size_paragraph, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, NULL);

		char paragraph[size_paragraph];
		MPI_Recv(&paragraph, size_paragraph, MPI_CHAR, 0, 0, MPI_COMM_WORLD, NULL);

		current_paragraph = tokenize_string(string(paragraph, size_paragraph), '\n');
		int nr_lines = current_paragraph.size();

		if ((sysconf(_SC_NPROCESSORS_CONF) - 1) * 20 < nr_lines) {
			P = sysconf(_SC_NPROCESSORS_CONF);
		} else {
			P = nr_lines / 20 + 1;
		}

		if (P >= 2) {
			P--;
		};

		pthread_barrier_init(&barrier_threads_processors, NULL, P);
		pthread_t threads_worker[P];

		for (int id = 0; id < P; ++id) {
			struct struct_thread_worker *s = (struct struct_thread_worker *) malloc(sizeof(struct struct_thread_worker));
        	s->rank = st.rank;
        	s->id = id;
        	s->nrLines = nr_lines;
        	s->maxThreads = P;
        	int r = pthread_create(&threads_worker[id], NULL, thread_worker_processor, (void *) s);
			
			if (r) {
	            printf("Eroare la crearea thread-ului %d\n", id);
	            exit(-1);
	        }
		}

		for (int id = 0; id < P; ++id) {
			int r = pthread_join(threads_worker[id], NULL);
	 
	        if (r) {
	            printf("Eroare la asteptarea thread-ului %d\n", id);
	            exit(-1);
	        }
		}
	}
	pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
	int provided;
	MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
	int numtasks, rank, len;
	char hostname[MPI_MAX_PROCESSOR_NAME];
	MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Get_processor_name(hostname, &len);
    pthread_t threads_master[NUM_THREADS];
    pthread_t thread_worker;

	if (rank == 0) {
		pthread_barrier_init(&threads_worker_barrier, NULL, NUM_THREADS);
		for (int id = 0; id < NUM_THREADS; id++) {
		    struct struct_thread_master *s = (struct struct_thread_master *) malloc(sizeof(struct struct_thread_master));
		    char* fileName = argv[1];
            s->fileName = fileName;
		    s->id = id;

	        int r = pthread_create(&threads_master[id], NULL, thread_master_function, (void*) s);
	 
	        if (r) {
	            printf("Eroare la crearea thread-ului %d\n", id);
	            exit(-1);
	        }
	    }
	} else {
		struct struct_thread_worker *s = (struct struct_thread_worker *) malloc(sizeof(struct struct_thread_worker));
        s->rank = rank;
        s->id = 0;
        int r = pthread_create(&thread_worker, NULL, thread_worker_receiver, (void *) s);
 
        if (r) {
            printf("Eroare la crearea thread-ului %d\n", s->id);
            exit(-1);
        }	

        r = pthread_join(thread_worker, NULL);
	}

	if (rank == 0) {
    	for (int id = 0; id < NUM_THREADS; id++) {
	       int r = pthread_join(threads_master[id], NULL);
	 
	        if (r) {
	            printf("Eroare la asteptarea thread-ului %d\n", id);
	            exit(-1);
	        }
    	}
    }

	MPI_Finalize();
    return 0;
}