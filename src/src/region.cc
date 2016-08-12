#include "region.h"
#include "config.h"
#include "util.h"
#include <algorithm>

using namespace std;

region::region(int32_t _lpos, int32_t _rpos, int _ltype, int _rtype, const split_interval_map *_imap, const split_interval_map *_jmap)
	:lpos(_lpos), rpos(_rpos), imap(_imap), jmap(_jmap), ltype(_ltype), rtype(_rtype)
{
	empty = false;

	tie(lit, rit) = locate_boundary_iterators(*imap, lpos, rpos);
	if(lit == imap->end() || rit == imap->end()) empty = true;

	check_left_region();
	check_right_region();
	if(lmid == lpos && rmid == rpos) empty = true;

	print(99);
	if(lmid != rpos) assert(lmid < rmid);
	if(rmid != lpos) assert(lmid < rmid);
}

region::~region()
{}

int region::check_left_region()
{
	lmid = lpos;
	if(empty == true) return 0;

	int32_t epsilon = 1;
	if(ltype == RIGHT_SPLICE) lmid = lpos + epsilon;
	else if(lpos != lower(lit->first)) return 0;

	SIMI it = lit;
	while(true)
	{
		if(lower(it->first) > lmid + epsilon) break;
		lmid = upper(it->first);
		if(it == rit) break;
		it++;
	}

	if(lmid >= rpos) lmid = rpos;
	if(lmid == rpos) return 0;

	bool b = empty_subregion(lpos, lmid);
	if(b == true && ltype != RIGHT_SPLICE) lmid = lpos;

	if(ltype == RIGHT_SPLICE) assert(lmid > lpos);

	return 0;
}

int region::check_right_region()
{
	rmid = rpos;
	if(empty == true) return 0;

	int32_t epsilon = 1;
	if(rtype == LEFT_SPLICE) rmid = rpos - epsilon;
	else if(rpos != upper(rit->first)) return 0;

	SIMI it = rit;
	while(true)
	{
		if(upper(it->first) < rmid - epsilon) break;
		rmid = lower(it->first);
		if(it == lit) break;
		it--;
	}

	if(rmid <= lpos) rmid = lpos;
	if(rmid == lpos) return 0;

	bool b = empty_subregion(rmid, rpos);
	if(b == true && rtype != LEFT_SPLICE) rmid = rpos;

	if(rtype == LEFT_SPLICE) assert(rmid < rpos);

	return 0;
}

bool region::empty_subregion(int32_t p1, int32_t p2)
{
	assert(p1 < p2);
	assert(p1 >= lpos && p2 <= rpos);

	if(p2 - p1 < min_subregion_length) return true;

	SIMI it1, it2;
	tie(it1, it2) = locate_boundary_iterators(*imap, p1, p2);
	if(it1 == imap->end() || it2 == imap->end()) return true;

	int32_t sum = compute_sum_overlap(*imap, it1, it2);
	double ratio = sum * 1.0 / (p2 - p1);
	if(ratio < min_average_overlap) return true;

	int32_t indel = 0;
	SIMI jit1 = jmap->lower_bound(ROI(p1, p1 + 1));
	SIMI jit2 = jmap->upper_bound(ROI(p2 - 1, p2));
	for(SIMI jit = jit1; jit != jit2; jit++) indel += jit->second;

	if(indel * 1.0 * ratio > max_indel_ratio) return true;

	return false;
}

int region::build_partial_exons(vector<partial_exon> &pexons)
{
	pexons.clear();

	if(empty == true) return 0;
	if(lmid != rpos) assert(lmid < rmid);
	if(rmid != lpos) assert(lmid < rmid);

	if(lmid == rpos)
	{
		partial_exon pe(lpos, rpos, ltype, rtype);
		evaluate_rectangle(*imap, pe.lpos, pe.rpos, pe.ave, pe.dev);
		pexons.push_back(pe);
		return 0;
	}

	if(lmid == lpos)
	{
		assert(rmid < rpos);
		assert(rmid > lpos);
		partial_exon pe(rmid, rpos, START_BOUNDARY, rtype);
		evaluate_rectangle(*imap, pe.lpos, pe.rpos, pe.ave, pe.dev);
		pexons.push_back(pe);
		return 0;
	}

	assert(lpos < lmid && lmid < rpos);

	if(lpos < lmid)
	{
		partial_exon pe(lpos, lmid, ltype, END_BOUNDARY);
		evaluate_rectangle(*imap, pe.lpos, pe.rpos, pe.ave, pe.dev);
		pexons.push_back(pe);
	}

	if(rmid < rpos)
	{
		assert(lmid <= rmid);
		partial_exon pe(rmid, rpos, START_BOUNDARY, rtype);
		evaluate_rectangle(*imap, pe.lpos, pe.rpos, pe.ave, pe.dev);
		pexons.push_back(pe);
	}

	return 0;
}

int region::print(int index) const
{
	int32_t lc = compute_overlap(*imap, lpos);
	int32_t rc = compute_overlap(*imap, rpos - 1);
	printf("region %d: empty = %c, type = (%d, %d), pos = [%d, %d), mid = [%d, %d), boundary coverage = (%d, %d)\n", 
			index, empty ? 'T' : 'F', ltype, rtype, lpos, rpos, lmid, rmid, lc, rc);
	return 0;
}
