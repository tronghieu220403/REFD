#include "matcher.h"

#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <limits>

void Matcher::SetInput(
    const std::vector<std::vector<std::string>>& vectors,
    const std::vector<std::string>& b_strings) {
    num_vectors_ = static_cast<int>(vectors.size());
    num_b_strings_ = static_cast<int>(b_strings.size());

    // Build map from string to list of vector indices that contain it.
    std::unordered_map<std::string, std::vector<int>> string_to_vector_indices;
    string_to_vector_indices.reserve(num_vectors_ * 2);

    for (int vector_index = 0; vector_index < num_vectors_; ++vector_index) {
        std::unordered_set<std::string> seen;
        for (const auto& s : vectors[vector_index]) {
            if (seen.insert(s).second) {
                string_to_vector_indices[s].push_back(vector_index);
            }
        }
    }

    // Build adjacency.
    adj_.assign(num_b_strings_, std::vector<int>());
    for (int b_index = 0; b_index < num_b_strings_; ++b_index) {
        const auto& b_str = b_strings[b_index];
        auto it = string_to_vector_indices.find(b_str);
        if (it != string_to_vector_indices.end()) {
            adj_[b_index] = it->second;
        }
    }
}

bool Matcher::Bfs() {
    std::queue<int> bfs_queue;
    const int kInfDistance = INT_MAX;
    dist_.assign(num_b_strings_, kInfDistance);

    // All free b-vertices are distance 0.
    for (int b_index = 0; b_index < num_b_strings_; ++b_index) {
        bool is_matched = false;
        for (int vector_index : adj_[b_index]) {
            if (match_right_[vector_index] == b_index) {
                is_matched = true;
                break;
            }
        }
        if (!is_matched) {
            dist_[b_index] = 0;
            bfs_queue.push(b_index);
        }
    }

    bool reachable_free = false;
    while (!bfs_queue.empty()) {
        int b_index = bfs_queue.front();
        bfs_queue.pop();
        int next_dist = dist_[b_index] + 1;
        for (int vector_index : adj_[b_index]) {
            int matched_b = match_right_[vector_index];
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

bool Matcher::Dfs(int b_index) {
    for (int vector_index : adj_[b_index]) {
        int matched_b = match_right_[vector_index];
        if (matched_b == -1 ||
            (dist_[matched_b] == dist_[b_index] + 1 && Dfs(matched_b))) {
            match_right_[vector_index] = b_index;
            return true;
        }
    }
    dist_[b_index] = std::numeric_limits<int>::max();
    return false;
}

int Matcher::Solve() {
    match_right_.assign(num_vectors_, -1);
    int matching_count = 0;

    // Repeatedly find augmenting paths
    while (Bfs()) {
        for (int b_index = 0; b_index < num_b_strings_; ++b_index) {
            // Check if b_index is free (not matched as right partner)
            bool is_matched = false;
            for (int vector_index : adj_[b_index]) {
                if (match_right_[vector_index] == b_index) {
                    is_matched = true;
                    break;
                }
            }
            if (!is_matched && Dfs(b_index)) {
                matching_count++;
            }
        }
    }

    return matching_count;
}