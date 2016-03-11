#include <cstdio>
#include <cassert>
#include <sstream>

#include "config.h"
#include "manager.h"
#include "bundle.h"
#include "stringtie.h"
#include "scallop1.h"
#include "scallop2.h"
#include "scallop3.h"
#include "gtf_gene.h"
#include "boost_graph.h"

manager::manager()
{
}

manager::~manager()
{
}

int manager::process(const string &file)
{
	string s = file.substr(file.size() - 3, 3);
	if(s == "bam" || s == "sam") assemble_bam(file);
	else if(s == "gtf") assemble_gtf(file);
	else assemble_example(file);
	return 0;
}

int manager::assemble_bam(const string &file)
{
    samFile *fn = sam_open(file.c_str(), "r");
    bam_hdr_t *h= sam_hdr_read(fn);
    bam1_t *b = bam_init1();

	ofstream stringtie_fout("stringtie.gtf");
	ofstream scallop1_fout("scallop1.gtf");

	int index = 0;
	bundle_base bb;
    while(sam_read1(fn, h, b) >= 0)
	{
		bam1_core_t &p = b->core;
		if((p.flag & 0x4) >= 1) continue;		// read is not mapped, TODO
		if((p.flag & 0x100) >= 1) continue;		// secondary alignment
		if(p.n_cigar < 1) continue;				// should never happen
		if(p.n_cigar > 7) continue;				// ignore hits with more than 7 cigar types
		//if(p.qual <= 4) continue;				// ignore hits with quality-score < 5
		if(bb.get_num_hits() > 0 && (bb.get_rpos() + min_bundle_gap < p.pos || p.tid != bb.get_tid()))
		{
			if(bb.get_num_hits() < min_num_hits_in_bundle) 
			{
				bb.clear();
				continue;
			}

			bundle bd(bb);
			bd.print(index);
			
			splice_graph gr;
			bd.build_splice_graph(gr);

			stringtie st(gr);
			st.assemble();
			st.print("stringtie");

			scallop1 sc(gr);
			sc.assemble();
			sc.print("scallop1");

			bd.output_gtf(stringtie_fout, st.paths, "stringtie", index);
			bd.output_gtf(scallop1_fout, sc.paths, "scallop1", index);

			index++;
			bb.clear();
			if(max_num_bundles > 0 && index > max_num_bundles) break;
		}
		bb.add_hit(h, b);
    }

	stringtie_fout.close();
	scallop1_fout.close();

    bam_destroy1(b);
    bam_hdr_destroy(h);
    sam_close(fn);

	return 0;
}

int manager::assemble_gtf(const string &file)
{
	ifstream fin(file.c_str());
	if(fin.fail())
	{
		printf("open file %s error\n", file.c_str());
		return 0;
	}

	char line[102400];
	
	vector<gtf_gene> genes;
	map<string, int> m;
	while(fin.getline(line, 102400, '\n'))
	{
		gtf_exon ge(line);
		if(ge.feature != "exon") continue;
		if(m.find(ge.gene_id) == m.end())
		{
			gtf_gene gg;
			gg.add_exon(ge);
			genes.push_back(gg);
			m.insert(pair<string, int>(ge.gene_id, genes.size() - 1));
		}
		else
		{
			genes[m[ge.gene_id]].add_exon(ge);
		}
	}

	/*
	ofstream stringtie_fout("stringtie.gtf");
	ofstream scallop1_fout("scallop1.gtf");
	ofstream scallop2_fout("scallop2.gtf");
	ofstream standard_fout("standard.gtf");
	*/

	for(int i = 0; i < genes.size(); i++)
	{
		gtf_gene &gg = genes[i];
		if(gg.exons.size() <= 0) continue;

		// DEBUG
		if(gg.exons[0].gene_id != "ABI3BP") continue;

		splice_graph gr;
		gg.build_splice_graph(gr);

		string s;	
		int p = compute_num_paths(gr);
		assert(p >= num_edges(gr) - num_vertices(gr) + 2);
		if(p == num_edges(gr) - num_vertices(gr) + 2) s = "EASY";
		else s = "HARD";

		if(s == "EASY") continue;

		scallop3 sc(gg.exons[0].gene_id, gr);
		sc.assemble();
		//gg.output_gtf(scallop2_fout, sc.paths, "scallop2");

		continue;

		bool b = check_nested_splice_graph(sc.gr);

		printf("gene %s, %lu transcipts, total %lu exons, %lu vertices, %lu edges %d paths, %s, %s\n",
				gg.exons[0].gene_id.c_str(), gg.transcripts.size(), gg.exons.size(),
				num_vertices(sc.gr), num_edges(sc.gr), p, s.c_str(), b ? "NESTED" : "GENERAL");


		char buf[1024];
		sprintf(buf, "%s.0.tex", gg.exons[0].gene_id.c_str());
		draw_splice_graph(gr, buf, 1.4);

		sprintf(buf, "%s.1.tex", gg.exons[0].gene_id.c_str());
		draw_splice_graph(sc.gr, buf, 2.4);

		/*
		gg.output_gtf(standard_fout);

		scallop2 sc(gr);
		sc.assemble();
		gg.output_gtf(scallop2_fout, sc.paths, "scallop2");

		stringtie st(gr);
		st.assemble();
		gg.output_gtf(stringtie_fout, st.paths, "stringtie");
		*/
	}

	/*
	stringtie_fout.close();
	scallop1_fout.close();
	scallop2_fout.close();
	standard_fout.close();
	*/

	return 0;
}

int manager::assemble_example(const string &file)
{
	splice_graph gr;
	build_splice_graph(gr, file);
	draw_splice_graph(gr, "splice_graph.tex");

	scallop2 sc(gr);
	sc.assemble();
	sc.print("scallop2");

	return 0;
}
