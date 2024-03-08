#include <vector>
#include <iostream>
#include <string>
#include <unordered_map>
#include <algorithm>
using namespace std;

// check if row above and below current are same
vector <int> findStartingRow(vector <int> &v){
    vector <int> rv;
    rv.resize(v.size() + 1, 0);

        rv[0] = 1;

    for (int i = 1; i < v.size(); i++)
    {
        int c = i-1; // rows above current row
        for (int j = i; j< v.size(); j++)
        {
            
            if (v[c] != v[j]){
                break;
            }
            else {
                if (rv[c] == 1){
                    rv[i] = 1;
                    break;
                }

            }
            c--;
            
        }
    }
    return rv;
}


int main()
{
    string s;
   
    vector<string> v;
    unordered_map<string, int> row;
    unordered_map<string, int> col;
    unordered_map<string, int>::iterator mit;
    s.clear();
    while (getline(cin, s))
    {
        v.push_back(s);
    }
   
    
    vector <int> cRow;
   vector <int> RcRow;
    vector <int> cCol;
    vector <int> RcCol;

    // compress the graph along row
    for (int i = 0; i < v.size(); i++){
        if (row.find(v[i]) == row.end()){
            row.insert(make_pair(v[i], i));
        }
        // top -> bottom
        cRow.push_back(row[v[i]]);
    }
    // bottom -> top
   for (int i = cRow.size()-1; i >= 0; i --) RcRow.push_back(cRow[i]);

    // compress along columns
    string ss;
    for (int i = 0; i < v[0].size(); i++){
        ss = "";
        for (int j = 0; j < v.size(); j ++){
            ss += (v[j][i]);
        }
        if (col.find(ss) == col.end()){
            col.insert(make_pair(ss, i));
        }
        // left -> right
        cCol.push_back(col[ss]);
    }
    // right -> left
    for (int i = cCol.size()-1; i >= 0; i --) {
        RcCol.push_back(cCol[i]);
    }
	
    vector <int> top = findStartingRow(cRow);

    vector <int> bottom = findStartingRow(RcRow);
    vector <int> left = findStartingRow(cCol);
    vector <int> right = findStartingRow(RcCol);
    
    vector <int> cp_right; 
    vector <int> cp_bottom; 


    // reverse order of bottom and right vectors
    for (int i = bottom.size()-1; i >= 0; i--) cp_bottom.push_back(bottom[i]);
    for (int i = right.size()-1; i >= 0; i--) cp_right.push_back(right[i]);
    
    long long count = 0; 
    long long count2 = 0; 
    
    // find overlapping 1
    for (int i= 0; i < top.size(); i ++){
        for (int j = i+1; j < top.size(); j++){
            if (top[i] == 1 && cp_bottom[j] == 1) {
                count ++;
            }
        }
    }
    for (int i= 0; i < left.size(); i ++){
        for (int j = i+1; j < left.size(); j++){
            if (left[i] == 1 && cp_right[j] == 1) {
                count2 ++;
            }
        }
    }
    
    long long a= count * count2 ;
    printf("%lld\n",a);
    return 0;
    
}

