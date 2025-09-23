#ifndef MATCHER_H_
#define MATCHER_H_
#include <vector>
#include <string>

// Matcher finds maximum matching between strings B and vectors,
// where each element in B can match at most one vector and vice versa.
class Matcher {
public:
    Matcher() = default;
    ~Matcher() = default;

    // Sets inputs: `vectors` is list of vectors of wstring; `b_strings`
    // is list of strings to match.
    void SetInput(
        const std::vector<std::vector<std::wstring>>& vectors,
        const std::vector<std::wstring>& b_strings);

    // Returns maximum number of matches.
    int Solve();

private:
    // Adjacency list: for each b index, vector of vector indices it can match.
    std::vector<std::vector<int>> adj_;

    // match of right side (vectors). For each vector index, which b it is matched to, or -1.
    std::vector<int> match_right_;

    // Distance (levels) for BFS, for each left node b.
    std::vector<int> dist_;

    int num_b_strings_ = 0;
    int num_vectors_ = 0;

    bool Bfs();
    bool Dfs(int b_index);
};

#endif  // MATCHER_H_
