#include "matcher.h"

void Matcher::SetInput(
    const vector<vector<string>>& vectors,
    const vector<vector<string>>& b_strings) {
    num_vectors_ = static_cast<int>(vectors.size());
    num_b_strings_ = static_cast<int>(b_strings.size());

    // Build map from string to list of vector indices that contain it.
    unordered_map<string, vector<int>> string_to_vector_indices;
    string_to_vector_indices.reserve((size_t)num_vectors_ * 2);

    for (int i = 0; i < num_vectors_; ++i) {
        unordered_set<string> seen;
        for (const auto& s : vectors[i]) {
            if (seen.insert(s).second) {
                string_to_vector_indices[s].push_back(i);
            }
        }
    }

    // Build adjacency.
    adj_.assign(num_b_strings_, vector<int>());
    for (int i = 0; i < num_b_strings_; ++i) {
        for (const auto& str : b_strings[i])
        {
            auto it = string_to_vector_indices.find(str);
            if (it != string_to_vector_indices.end()) {
                ulti::AddVectorsInPlace(adj_[i], it->second);
            }
        }
        set<int> s(adj_[i].begin(), adj_[i].end());
        vector<int> v(s.begin(), s.end());
        adj_[i] = std::move(v);
    }
}

bool Matcher::Bfs() {
    queue<int> bfs_queue;
    const int kInfDistance = INT_MAX;
    dist_.assign(num_b_strings_, kInfDistance);

    // All free b-vertices are distance 0.
    for (int i = 0; i < num_b_strings_; ++i) {
        bool is_matched = false;
        for (int i : adj_[i]) {
            if (match_right_[i] == i) {
                is_matched = true;
                break;
            }
        }
        if (!is_matched) {
            dist_[i] = 0;
            bfs_queue.push(i);
        }
    }

    bool reachable_free = false;
    while (!bfs_queue.empty()) {
        int i = bfs_queue.front();
        bfs_queue.pop();
        int next_dist = dist_[i] + 1;
        for (int i : adj_[i]) {
            int matched_b = match_right_[i];
            if (matched_b == -1) {
                reachable_free = true;
            }
            else if (dist_[matched_b] == kInfDistance) {
                dist_[matched_b] = next_dist;
                bfs_queue.push(matched_b);
            }
        }
    }
    return reachable_free;
}

bool Matcher::Dfs(int i) {
    for (int i : adj_[i]) {
        int matched_b = match_right_[i];
        if (matched_b == -1 ||
            (dist_[matched_b] == dist_[i] + 1 && Dfs(matched_b))) {
            match_right_[i] = i;
            return true;
        }
    }
    dist_[i] = numeric_limits<int>::max();
    return false;
}

int Matcher::Solve() {
    match_right_.assign(num_vectors_, -1);
    int matching_count = 0;

    // Repeatedly find augmenting paths
    while (Bfs()) {
        for (int i = 0; i < num_b_strings_; ++i) {
            // Check if i is free (not matched as right partner)
            bool is_matched = false;
            for (int i : adj_[i]) {
                if (match_right_[i] == i) {
                    is_matched = true;
                    break;
                }
            }
            if (!is_matched && Dfs(i)) {
                matching_count++;
            }
        }
    }

    return matching_count;
}