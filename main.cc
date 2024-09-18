#include <iostream>
#include <fstream>
#include <unordered_set>
#include <string>
#include <future>
#include <vector>
#include <algorithm>
#include <sstream>

class WordProcessor {
public:
    WordProcessor(int num_threads) : num_threads(num_threads) {}

    void process_file(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + filename);
        }

        file.seekg(0, std::ios::end);
        std::streamsize file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::streamsize chunk_size = file_size / num_threads;

        std::vector<std::future<std::unordered_set<std::string>>> futures;

        for (int i = 0; i < num_threads; ++i) {
            futures.push_back(std::async(std::launch::async, &WordProcessor::process_chunk, this, std::ref(file), chunk_size, i == num_threads - 1));
        }

        for (auto& fut : futures) {
            auto local_set = fut.get();
            words_.insert(local_set.begin(), local_set.end());
        }

        file.close();
    }

    void print_result() const {
        std::cout << words_.size() << " unique words found.\n";
    }

private:
    int num_threads;
    std::unordered_set<std::string> words_;
    std::mutex file_mutex_;

    std::unordered_set<std::string> process_chunk(std::ifstream& file, std::streamsize chunk_size, bool is_last_chunk) {
        std::string buffer;
        buffer.reserve(chunk_size);

        {
            std::lock_guard<std::mutex> lock(file_mutex_);
            if (is_last_chunk) {
                buffer.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
            } else {
                buffer.resize(chunk_size);
                file.read(&buffer[0], chunk_size);
                adjust_boundary(file, buffer);
            }
        }

        return extract_unique_words(buffer);
    }

    void adjust_boundary(std::ifstream& file, std::string& buffer) {
        char c;
        while (file.get(c) && c != ' ') {
            buffer += c;
        }
    }

    std::unordered_set<std::string> extract_unique_words(const std::string& chunk) {
        std::unordered_set<std::string> words;
        std::istringstream stream(chunk);
        std::string word;

        while (stream >> word) {
            clean_word(word);
            if (!word.empty()) {
                words.insert(word);
            }
        }

        return words;
    }

    void clean_word(std::string& word) {
        word.erase(std::remove_if(word.begin(), word.end(), [](unsigned char c) {
            return !std::isalnum(c);
        }), word.end());

        std::transform(word.begin(), word.end(), word.begin(), ::tolower);
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <filename> [num_threads]\n";
        return 1;
    }

    std::string filename = argv[1];
    int num_threads = (argc >= 3) ? std::stoi(argv[2]) : std::thread::hardware_concurrency();

    if (num_threads <= 0) {
        std::cerr << "Invalid number of threads.\n";
        return 1;
    }

    try {
        WordProcessor processor(num_threads);
        processor.process_file(filename);
        processor.print_result();
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }

    return 0;
}
