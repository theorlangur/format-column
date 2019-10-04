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

struct line
{
    std::string l;
    std::vector<strSpan> columns;
    bool rn;
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
    char opt_sep = 0;
    char opt_out_sep = 0;
    bool space_after_sep = true;
    bool first_spaces = true;
    bool check_groups = true;

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
            opt_sep = argv[++i][0];
        }
        else if (std::strcmp(argv[i], "-osep") == 0)
        {
            opt_out_sep = argv[++i][0];
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

    char sep = opt_sep ? opt_sep : ',';
    char out_sep = opt_out_sep ? opt_out_sep : sep;

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

    auto is_new_column =[&](char c) { return  (c == sep) && groups_ok(c); };

    auto new_line = [&]{
        lines.push_back({});
        auto &line = lines.back();
        line.l.reserve(1024);//default 1024 symbols per line
        line.columns.reserve(cols.size());
        line.columns.push_back({0, 0});
        line.rn = false;

        reset_groups();
    };

    auto update_last_column = [&]{
        auto &line = lines.back();
        auto &c = line.columns.back();
        char lc = line.l.back();
        if (!is_new_column(lc) && !std::isspace(lc))
            c.e = line.l.size();

        if (c.b < line.l.size() && std::isspace(line.l.at(c.b)))//ommiting spaces
        {
            c.b = line.l.size() - 1;
            if (std::isspace(lc)) c.e = c.b;
        }
    };

    auto update_max_column = [&]{
        auto &line = lines.back();
        auto &c = line.columns.back();
        if (c.size() > cols[line.columns.size() - 1])
            cols[line.columns.size() - 1] = c.size();
    };

    auto new_column = [&]{
        auto &line = lines.back();
        line.columns.push_back({line.l.size(), line.l.size()});
        while (cols.size() < line.columns.size())
            cols.push_back(0);
    };

    new_line();

    const uint8_t bom_utf8[] = {0xef, 0xbb, 0xbf, 0};
    const uint8_t *pCurBom = bom_utf8;
    bool withBom = false;

    bool count_spaces_at_begin = true;
    int spaces_at_begin = 0;
    int max_space_count = 0;

    while(!is.eof())
    {
        char c;
        is.get(c);
        if (pCurBom)
        {
            if (*pCurBom++ == (uint8_t)c)
                continue;//just ommiting

            withBom = *(pCurBom - 1);
            pCurBom = nullptr;
        }
        auto &line = lines.back();
        line.l += c;
        if (first_spaces && count_spaces_at_begin)
        {
            if (std::isspace(c))
            {
                ++spaces_at_begin;
            }else
            {
                count_spaces_at_begin = false;
                if (spaces_at_begin > max_space_count)
                    max_space_count = spaces_at_begin;

                spaces_at_begin = 0;
            }
        }
        process_group(c, prev);
        update_last_column();

        if (c == '\n')
        {
            //update last columns
            if (prev == '\r')
                line.rn = true;

            update_max_column();
            new_line();
            count_spaces_at_begin = true;
        }else
        {
            if (is_new_column(c))
            {
                update_max_column();
                new_column();
            }
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
        bool is_last = &l == &lines.back();
        if (!is_last && first_spaces)
            os.write(&*spaces.begin(), max_space_count);

        size_t n = l.columns.size();
        for(size_t i = 0; i < n; ++i)
        {
            size_t m = cols[i];
            if (i)
            {
                os.put(out_sep);
                if (space_after_sep) os.put(' ');
            }
            if (l.columns[i].size())
                out_part(os, l.l, l.columns[i], m);
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
