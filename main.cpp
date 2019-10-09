#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>

using columns_t = std::vector<size_t>;//each element - corresponding max width of column in symbols

struct strSpan
{
    size_t b;
    size_t e;

    size_t size() const { return e - b; }
};

struct column_desc
{
	strSpan s;
	char sep;
};

struct line
{
    std::string l;
    std::vector<column_desc> columns;
    bool rn;
	bool ignore;
};

struct group_desc
{
    char b;
    char e;
    bool esc;
    int limit;
    int current;

    bool ok() const { return current <= limit;}
};

int main(int argc, const char *argv[])
{
    std::vector<line> lines;
    columns_t cols;

    group_desc g_descs[] = { 
        {'(' , ')' , false, 0, 0}, //0
        {'[' , ']' , false, 0, 0}, //1
        {'{' , '}' , false, 0, 0}, //2
        {'<' , '>' , false, 0, 0}, //3
        {'"' , '"' , true , 0, 0}, //4
        {'\'', '\'', true , 0, 0}, //5
    };

	char separators[256] = { 0 };

	auto is_sep = [&](char c) { return separators[uint8_t(c)] != 0; };
	auto get_out_sep = [&](char c) { return separators[uint8_t(c)]; };

    int groups[256];
    std::fill(groups, groups + 256, -1);
    
    groups['(']  = groups[')'] = 0;
    groups['[']  = groups[']'] = 1;
    groups['{']  = groups['}'] = 2;
    groups['<']  = groups['>'] = 3;
    groups['"']  = 4;         
    groups['\''] = 5;         

    std::fstream in_file;
    std::fstream out_file;

    bool smart = true;
	std::string opt_sep, opt_out_sep;
    bool space_after_sep = true;
    bool first_spaces = true;
    bool check_groups = true;
	std::string ignore_line_start_pattern("");

    for(int i = 0; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "-in") == 0)
        {
            in_file.open(argv[++i], std::ios::in);
        }else if (std::strcmp(argv[i], "-out") == 0)
        {
            out_file.open(argv[++i], std::ios::out);
        }else if (std::strcmp(argv[i], "-sep") == 0)
        {
            opt_sep = argv[++i];
        }
        else if (std::strcmp(argv[i], "-osep") == 0)
        {
            opt_out_sep = argv[++i];
        }
        else if (std::strcmp(argv[i], "-ignore") == 0)
        {
            ignore_line_start_pattern = argv[++i];
        }
        else if (std::strcmp(argv[i], "-nosmart") == 0)
        {
            smart = false;
        }
        else if (std::strcmp(argv[i], "-nogroups") == 0)
        {
            check_groups = false;
            smart = false;
        }
        else if (std::strcmp(argv[i], "-nosepspace") == 0)
        {
            space_after_sep = false;
        }
        else if (std::strcmp(argv[i], "-nofirstspace") == 0)
        {
            first_spaces = false;
        }
        else if (std::strcmp(argv[i], "-depthcfg") == 0)//config in form of [<sym><limit>]+ (e.g. {2 - meaning limit of 2 for grouping {})
        {
            ++i;
            const char *cfg = argv[i];
            while(*cfg)
            {
              char c = cfg[0];
              if (c && (groups[c] != -1)) {
                int d = cfg[1];
                if (d >= '0' && d <= '9') {
                  d -= '0';
                  g_descs[groups[c]].limit = d;
                  cfg += 2;//advance to the next pair of cfg
                }else
                    break;
              }else
                break;
            }
        }
    }

    std::string sep = !opt_sep.empty() ? opt_sep : std::string(",");

	auto out_it = opt_out_sep.begin();
	for (char c : sep)
	{
		if (out_it != opt_out_sep.end())
			separators[c] = *out_it++;
		else
			separators[c] = c;
	}

    std::istream &is = in_file.is_open() ? in_file : std::cin;
    std::ostream &os = out_file.is_open() ? out_file : std::cout;

    cols.push_back(0);
    char prev = 0;
    char smart_c = 0;

    auto groups_ok = [&](char c){
        if (!check_groups) return true;
        for(const auto &d : g_descs) 
        {
            if (d.b == c || d.e == c)
                continue;

            if (!d.ok()) return false;
        }
        return true;
    };

    auto reset_groups = [&]{ for(auto &d : g_descs) d.current = 0; };

    auto process_group = [&](char c, char prev) {
      if (!check_groups) return;
      uint8_t u = (uint8_t)c;
      if (groups[u] != -1) 
      {
        // valid descriptor
        auto &gr = g_descs[groups[u]];
        if (gr.esc) {
          if (prev != '\\') {
            if (gr.current == 0) // opening
            {
              gr.current = 1;
              if (smart)
              {
                gr.limit = 1;
                smart = false;
              }
            }
            else {
              // close of the object
              gr.current = 0;
            }
          }
        } else {
          if (gr.b == c) {
            ++gr.current;
            if (smart)
            {
                if (!smart_c || (smart_c == c))
                {
                    gr.limit = gr.current;
                    smart_c = c;
                }else
                    smart = false;
            }
          } else {
            --gr.current;
            smart = false;
          }
        }
      }else if (smart && !std::isspace(c))
        smart = false;
    };

    auto is_new_column =[&](char c) { return  is_sep(c) && groups_ok(c); };

    auto new_line = [&]{
        lines.push_back({});
        auto &line = lines.back();
        line.l.reserve(1024);//default 1024 symbols per line
        line.columns.reserve(cols.size());
        line.columns.push_back({0, 0});
        line.rn = false;
		line.ignore = false;

        reset_groups();
    };

    auto update_last_column = [&]{
        auto &line = lines.back();
        auto &c = line.columns.back();
        char lc = line.l.back();
        if (!is_new_column(lc) && !std::isspace(lc))
            c.s.e = line.l.size();

        if (c.s.b < line.l.size() && std::isspace(line.l.at(c.s.b)))//ommiting spaces
        {
            c.s.b = line.l.size() - 1;
            if (std::isspace(lc)) c.s.e = c.s.b;
        }
    };

    auto update_max_column = [&]{
        auto &line = lines.back();
        auto &c = line.columns.back();
        if (c.s.size() > cols[line.columns.size() - 1])
            cols[line.columns.size() - 1] = c.s.size();
    };

    auto new_column = [&](char c){
        auto &line = lines.back();
        line.columns.push_back({line.l.size(), line.l.size()});
		line.columns.back().sep = c;
        while (cols.size() < line.columns.size())
            cols.push_back(0);
    };

    new_line();

    const uint8_t bom_utf8[] = {0xef, 0xbb, 0xbf, 0};
    const uint8_t *pCurBom = bom_utf8;
    bool withBom = false;

	auto check_bom = [&](char c)
	{
		if (pCurBom)
		{
			if (*pCurBom++ == (uint8_t)c)
				return true;//just ommiting

			withBom = pCurBom == (bom_utf8 + 3);
			pCurBom = nullptr;
		}
		return false;
	};

    bool count_spaces_at_begin = true;
    int spaces_at_begin = 0;
    int max_space_count = 0;

	auto reset_spaces = [&] {
		count_spaces_at_begin = true;
		spaces_at_begin = 0;
	};

	auto count_spaces = [&](char c) {
        if (count_spaces_at_begin)
        {
            if (std::isspace(c))
            {
                ++spaces_at_begin;
            }else
            {
                count_spaces_at_begin = false;
                if (first_spaces && (spaces_at_begin > max_space_count))
                    max_space_count = spaces_at_begin;
            }
        }
	};

	bool check_ignore_line_pattern = !ignore_line_start_pattern.empty();
	const char* pIgnorePatternCur = &*ignore_line_start_pattern.begin();

	auto reset_ignore_state = [&] {
		check_ignore_line_pattern = !ignore_line_start_pattern.empty();
		pIgnorePatternCur = &*ignore_line_start_pattern.begin();
	};

	auto check_ignore = [&](char c)
	{
		if (check_ignore_line_pattern && !std::isspace(c))
		{
			auto& line = lines.back();
			if (c != *pIgnorePatternCur)
			{
				check_ignore_line_pattern = false;//don't check until the new line
			}
			else
			{
				++pIgnorePatternCur;
				if (*pIgnorePatternCur == 0)//end of a pattern?
				{
					line.ignore = true;
					check_ignore_line_pattern = false;
				}
			}
		}
	};


    while(!is.eof())
    {
        char c;
        is.get(c);
		if (check_bom(c)) continue;
        auto &line = lines.back();
        line.l += c;
		count_spaces(c);
		check_ignore(c);

		if (!line.ignore)
		{
			process_group(c, prev);
			update_last_column();
		}

        if (c == '\n')
        {
            //update last columns
            if (prev == '\r')
                line.rn = true;

			if (!line.ignore)
				update_max_column();

            new_line();
			reset_spaces();
			reset_ignore_state();
        }
		else if (!line.ignore && is_new_column(c))
		{
			update_max_column();
			new_column(c);
		}

        prev = c;
    }

    auto out_part = [&](std::ostream &os, const std::string &str, strSpan const &s, size_t m)
    {
        size_t subSize = s.size();
        os.write(str.c_str() + s.b, subSize);
        size_t d = m - subSize;
        while(d--) os.put(' ');
    };

    if (lines.back().l.empty())
        lines.pop_back();

    if (withBom) os.write((const char*)bom_utf8, 3);

    std::vector<char> spaces(max_space_count, ' ');

    //output now all
    for(auto &l : lines)
    {
		if (l.ignore)
		{
			os << l.l;//output as-is
			continue;
		}

        bool is_last = &l == &lines.back();
        if (!is_last && first_spaces)
            os.write(&*spaces.begin(), max_space_count);

        size_t n = l.columns.size();
        for(size_t i = 0; i < n; ++i)
        {
            size_t m = cols[i];
			auto const& col = l.columns[i];
            if (i)
            {
                os.put(get_out_sep(col.sep));
                if (space_after_sep) os.put(' ');
            }
            if (col.s.size())
                out_part(os, l.l, col.s, m);
        }
        if (!is_last)
        {
            if (l.rn)
                os.write("\r\n", 2);//writing either \r\n or \n or nothing
            else
                os.write("\n", 1);//writing either \r\n or \n or nothing
        }
    }

    os.flush();

    return 0;
}
