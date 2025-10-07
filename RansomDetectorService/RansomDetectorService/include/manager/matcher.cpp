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
    queue<int> q;
    const int kInfDistance = INT_MAX;
    dist_.assign(num_b_strings_, kInfDistance);

    // All free b-vertices are distance 0.
    for (int u = 0; u < num_b_strings_; ++u) {
        bool is_matched = false;
        for (int v : adj_[u]) {
            if (match_right_[v] == u) {
                is_matched = true;
                break;
            }
        }
        if (!is_matched) {
            dist_[u] = 0;
            q.push(u);
        }
    }

    bool reachable_free = false;
    while (!q.empty()) {
        int u = q.front(); q.pop();
        int nd = dist_[u] + 1;
        for (int v : adj_[u]) {
            int matched_u = match_right_[v];
            if (matched_u == -1) {
                reachable_free = true;
            }
            else if (dist_[matched_u] == kInfDistance) {
                dist_[matched_u] = nd;
                q.push(matched_u);
            }
        }
    }
    return reachable_free;
}

bool Matcher::Dfs(int u) {
    for (int v : adj_[u]) {
        int matched_u = match_right_[v];
        if (matched_u == -1 ||
            (dist_[matched_u] == dist_[v] + 1 && Dfs(matched_u))) {
            match_right_[v] = u;
            return true;
        }
    }
    dist_[u] = INT_MAX;
    return false;
}

int Matcher::Solve() {
    match_right_.assign(num_vectors_, -1);
    int matching_count = 0;

    // Repeatedly find augmenting paths
    while (Bfs()) {
        for (int u = 0; u < num_b_strings_; ++u) {
            // Check if i is free (not matched as right partner)
            bool is_matched = false;
            for (int v : adj_[u]) {
                if (match_right_[v] == u) {
                    is_matched = true;
                    break;
                }
            }
            if (!is_matched && Dfs(u)) {
                matching_count++;
            }
        }
    }

    return matching_count;
}