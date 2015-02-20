#include "toolset.h"

namespace ts {

template <typename TCHARACTER> bp_t<TCHARACTER> &bp_t<TCHARACTER>::add_block(const string_type &name)
{
	bool added;
	hashmap_t<string_type, bp_t<TCHARACTER>*>::litm_s &li = elements.addAndReturnItem(name, added);
	element_s *e = TSNEW(element_s);
	li.value = &e->bp;
	e->name = &li.key;
	e->next = nullptr;
	if (first_element == nullptr) first_element = e; else last_element->next = e;
	last_element = e;
	DEBUGCODE(if (!added) li.value = nullptr;) // mark as duplicate
	return e->bp;
}


template <typename TCHARACTER> int bp_t<TCHARACTER>::get_current_line(const TCHARACTER *s)
{
#ifdef _DEBUG
	if (source_basis && s)
	{
		int line = 1;
		for (const TCHARACTER *t = source_basis; t < s; t++) // calculate number of lines from start of buffer to current position
			if (*t == TCHARACTER('\r'))
			{
				if (t < s-1 && *(t+1)==TCHARACTER('\n')) t++; // assume \r\n as one line
				line++;
			}
			else if (*t == TCHARACTER('\n')) line++;
		return line;
	}
#endif
	return -1;
}


template<typename TCHARACTER> static const TCHARACTER *token_start(const TCHARACTER *t, const TCHARACTER *end)
{
#define TOKEN_CHECK(c) (c!=' ' && c!=TCHARACTER('\t') && c!=TCHARACTER('\r') && c!=TCHARACTER('\n'))
	for (;t<end-1;t++)
	{
		if (*t==TCHARACTER('/') && (*(t+1)==TCHARACTER('*') || *(t+1)==TCHARACTER('/')))
		{
			if (*(t+1)==TCHARACTER('*')) // multiline comment
			{
				for (t+=2; t<end-1; t++) // seek for comment end
					if (*t==TCHARACTER('*') && *(t+1)==TCHARACTER('/')) break;
				if (t==end-1) {WARNING("Unended comment"); break;}
				t++;
			}
			else
			{
				for (t+=2; t<end && *t!=TCHARACTER('\r') && *t!=TCHARACTER('\n'); t++);
				//t--;
			}
			continue;
		}
		if (TOKEN_CHECK(*t)) return t;
	}
	if (t<end && TOKEN_CHECK(*t)) return t;
#undef TOKEN_CHECK
	return nullptr;
}

template<typename TCHARACTER> static const TCHARACTER *token_end(const TCHARACTER *&s, const TCHARACTER *end, TCHARACTER addc)
{
	const TCHARACTER *start = s, *t;
	for (; s<end && *s!=TCHARACTER('\r') && *s!=TCHARACTER('\n') && *s!=TCHARACTER('{') && *s!=addc; s++)
		if (*s == '/' && s<end-1 && (*(s+1)==TCHARACTER('*') || *(s+1)==TCHARACTER('/'))) break;
	//END_CHECK("looking for end of line")
	for (t = s-1; t>start && (*t == TCHARACTER(' ') || *t == TCHARACTER('\t')); t--);

	s = token_start(s, end); // skip comments and empty lines
	return t + 1;
}

template <typename TCHARACTER> bool bp_t<TCHARACTER>::read_bp(const TCHARACTER *&s, const TCHARACTER *end)
{
	DEBUGCODE(if (!source_basis) source_basis = s;)
#define END_CHECK(msg) if (s >= end) {WARNING(str_t<char>("Unexpected eof while " msg "(line: ").append_as_int(get_current_line(s)).append_char(')').cstr()); return nullptr;}
#define SKIP_SEPARATORS(additional_check) \
	while (true)\
	{\
		/*END_CHECK("skipping separators")*/if (s >= end) break;\
		if (*s!=TCHARACTER(' ') && *s!=TCHARACTER('\t') && additional_check) break;\
		s++;\
	}

	const TCHARACTER *start = s;
	if ((s = token_start(s, end)) == nullptr) return false;
	if (preserve_comments && s > start) // keep comments if needed (only top level block)
	{
		string_type comment(start, s-start);
		if (comment.get_length() >= 2)
		{
			if (comment.get_last_char() == TCHARACTER('\n')) comment.trunc_length(); // remove last line
			if (comment.get_last_char() == TCHARACTER('\r')) comment.trunc_length();
			if (!comment.is_empty()) add_block(string_type().append_char(TCHARACTER(0))).value = comment;
		}
	}

	if (*s == TCHARACTER('}')) {++s; return false;}

	start = s;
	string_type name(start, token_end(s, end, TCHARACTER('=')) - start);
	bp_t<TCHARACTER> &bp = add_block(name);
	DEBUGCODE(bp.source_basis = source_basis;)
	//skip separators
	//SKIP_SEPARATORS(*s!=TCHARACTER('\r') && *s!=TCHARACTER('\n'))
	if (!s) return false;

	switch (*s)
	{
	case TCHARACTER('='):
		s++;
		NOWARNING(4127, SKIP_SEPARATORS(true))

		if (*s == TCHARACTER('`'))
		{
			string_type value;
			start = ++s;
			while (s<end)
			{
				if (*s == TCHARACTER('`'))
				{
					if (s<end-1 && *(s+1)==TCHARACTER('`')) // quoted '`'
					{
						value.append(sptr<TCHARACTER>(start, s+1-start));
						start = s+=2;
						continue;
					}
					else // line end
					{
						value.append(sptr<TCHARACTER>(start, s-start));
						break;
					}
				}
				s++;
			}
			bp.set_value(value);
			END_CHECK("looking for '`'")
			s++;
			SKIP_SEPARATORS(true)
		}
		else
		{
			start = s;
			const TCHARACTER *t;
			bp.set_value(sptr<TCHARACTER>(start, (t=token_end(s, end, TCHARACTER('}'))) - start));
			if (preserve_comments) // keep comments if needed
			{
				string_type comment(t, (!s?end:s)-t);
				if (comment.get_length() >= 2)
				{
					if (comment.get_last_char() == TCHARACTER('\n')) comment.trunc_length();
					if (comment.get_last_char() == TCHARACTER('\r')) comment.trunc_length();
					if (!comment.is_empty()) add_block(string_type(TCHARACTER(0))).value = comment;
				}
			}
			if (!s) return false;
		}
		if (!(s<end && *s==TCHARACTER('{'))) break;

	case TCHARACTER('{'):
		if ((s = bp.load(s+1, end)) == nullptr) return false;
		break;

	default:
		WARNING(str_t<char>("'=' or '{' expected (line: ").append_as_int(get_current_line(s)).append_char(')').cstr());
		return true;
	}
#undef SKIP_SEPARATORS
#undef END_CHECK

	return true;
}

template <typename TCHARACTER> const TCHARACTER *bp_t<TCHARACTER>::load(const TCHARACTER *data, const TCHARACTER *end)
{
	if (data)
	    while (read_bp(data, end));

	return data;
}


template <typename TCHARACTER> static const str_t<TCHARACTER> store_value(const str_t<TCHARACTER> &value, bool allow_unquoted = true)
{
	if (allow_unquoted && value.find_pos_of<char>(0, CONSTASTR(" \t\r\n`{}")) < 0) // any of these symbols mean string must be quoted
	{
		int i = 0;
		for (; i<value.get_length()-1; i++)
			if (value[i] == TCHARACTER('/') && (value[i+1] == TCHARACTER('/') || value[i+1] == TCHARACTER('*'))) break;
		if (i >= value.get_length()-1) return value;
	}
	str_t<TCHARACTER> r(value);
	r.replace_all(CONSTSTR(TCHARACTER,"`"), CONSTSTR(TCHARACTER,"``"));
	return TCHARACTER('`') + r + TCHARACTER('`');
}

template <typename TCHARACTER> const str_t<TCHARACTER> bp_t<TCHARACTER>::store(int level) const
{
	// can be block stored as single line?
	bool oneLine = true;
	if (elements.size() > 3 || level == 0 || preserve_comments)
		oneLine = false;
	else
	{
		int totalLen = value.get_length();
		for (element_s *e=first_element; e; e=e->next)
		{
			if (e->bp.first_element) {oneLine = false; break;} // no - there are inner blocks detected
			totalLen += e->name->get_length() + e->bp.value.get_length();
			if (totalLen > 40/*ONE_LINE_LIMIT*/) {oneLine = false; break;}
		}
	}
	// write
	string_type r;
	for (element_s *e=first_element; e; e=e->next)
	{
		if (*e->name == string_type(TCHARACTER(0))) {r += e->bp.value; continue;} // this is comment
		if (e->bp.value_not_specified() && !e->bp.first_element) {if (e == last_element) goto c_; continue;} // correct element skip
		if (!oneLine && (level > 0 || e!=first_element)) r.append(CONSTSTR(TCHARACTER,"\r\n")).append_chars(level, '\t');
		int prev_len = r.get_length();
		r += *e->name;
		if (!e->bp.value_not_specified()) r += TCHARACTER('=') + store_value(e->bp.value, !oneLine || e == last_element);// do not write = symbol if value not defined
		if (e->bp.first_element)// inner blocks?
		{
			if (r.get_length() > prev_len) r.append_char(' '); // to remove space in {a=3}\n {b=6}
			r.append_char('{').append(e->bp.store(level + 1)).append_char('}');
		}
		if (e != last_element) {if (oneLine) r.append_char(' ');}
		else {c_: if (!oneLine && level > 0) r.append(CONSTSTR(TCHARACTER,"\r\n")).append_chars(level-1, '\t');}
	}
	return r;
}


template class bp_t<char>;
template class bp_t<wchar>;

} // namespace ts