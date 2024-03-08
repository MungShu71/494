#include <string>
#include <vector>
#include <list>
#include <cmath>
#include <algorithm>
#include <map>
#include <set>
#include <iostream>
#include <cstdio>
#include <cstdlib>
using namespace std;

long long findMinimumValue(long long a, long long b){

	long long  x ,y ,z, left, mid, tmp, ap;

	y = a + b;
	tmp = z = sqrt(y);
	if (z * z != y) return -1;
	if (a == 2 || b == 2) return -1;
	if (a == 0) return 0;
/*
	n = 0;
	k = z - 1;
	for (i = z; i > 0; --i){
		if (x <= 0) break;
		j = i + k;
		k -= 1;
		if (x - j >= 0){
			x-=j;
			n++;
		}
	}

	return n;
*/	

	left = 0;
	while (left <= z){
	   mid =  (z + left) / 2;
 	   ap = y - ((tmp-mid) * (tmp-mid)); // points from last mid rounds
	   if (a - ap < 0 ) {z  = mid - 1; continue;}
	   if (ap ==  a ) return mid;
	    if ((a-ap < 2 * (tmp-mid)) && ((a-ap) % 2 == 1)) return mid + 1;
	    if ((a-ap <= 2 * (tmp-mid)) && ((a-ap) % 2 == 0)) return mid + 2; 
	    if (a - ap == 2 *(tmp-mid) + 1) return mid + 3;
	   else left = mid + 1;
	}
	
	  return mid;
	
}

int main(){
	long long a,b;
	cin >> a >> b;
	long long ans = findMinimumValue(a,b);
	cout << ans << endl;
	return 0;
}

