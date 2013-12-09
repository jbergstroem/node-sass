#include "ast.hpp"
#include "context.hpp"
#include "to_string.hpp"
#include <set>
#include <algorithm>
#include <iostream>

namespace Sass {
  using namespace std;

  bool Compound_Selector::operator<(const Compound_Selector& rhs) const
  {
    To_String to_string;
    // ugly
    return const_cast<Compound_Selector*>(this)->perform(&to_string) <
           const_cast<Compound_Selector&>(rhs).perform(&to_string);
  }

  bool Compound_Selector::is_superselector_of(Compound_Selector* rhs)
  {
    To_String to_string;

    Simple_Selector* lbase = base();
    Simple_Selector* rbase = rhs->base();

    set<string> lset, rset;
    for (size_t i = 1, L = length(); i < L; ++i)
    { lset.insert((*this)[i]->perform(&to_string)); }
    for (size_t i = 1, L = rhs->length(); i < L; ++i)
    { rset.insert((*rhs)[i]->perform(&to_string)); }

    return (!lbase || (rbase && (lbase->perform(&to_string) == rbase->perform(&to_string)))) &&
           // skip the pseudo-element check for now
           includes(rset.begin(), rset.end(), lset.begin(), lset.end());
  }

  bool Complex_Selector::is_superselector_of(Compound_Selector* rhs)
  {
    if (length() != 1)
    { return false; }
    return base()->is_superselector_of(rhs);
  }

  bool Complex_Selector::is_superselector_of(Complex_Selector* rhs)
  {
    Complex_Selector* lhs = this;
    // check for selectors with leading or trailing combinators
    if (!lhs->head() || !rhs->head())
    { return false; }
    Complex_Selector* l_innermost = lhs->innermost();
    if (l_innermost->combinator() != Complex_Selector::ANCESTOR_OF && !l_innermost->tail())
    { return false; }
    Complex_Selector* r_innermost = rhs->innermost();
    if (r_innermost->combinator() != Complex_Selector::ANCESTOR_OF && !r_innermost->tail())
    { return false; }
    // more complex (i.e., longer) selectors are always more specific
    size_t l_len = lhs->length(), r_len = rhs->length();
    if (l_len > r_len)
    { return false; }

    if (l_len == 1)
    { return lhs->head()->is_superselector_of(rhs->base()); }

    bool found = false;
    Complex_Selector* marker = rhs;
    for (size_t i = 0, L = rhs->length(); i < L; ++i) {
      if (i == L-1)
      { return false; }
      if (lhs->head()->is_superselector_of(marker->head()))
      { found = true; break; }
      marker = marker->tail();
    }
    if (!found)
    { return false; }

    /* 
      Hmm, I hope I have the logic right:

      if lhs has a combinator:
        if !(marker has a combinator) return false
        if !(lhs.combinator == '~' ? marker.combinator != '>' : lhs.combinator == marker.combinator) return false
        return lhs.tail-without-innermost.is_superselector_of(marker.tail-without-innermost)
      else if marker has a combinator:
        if !(marker.combinator == ">") return false
        return lhs.tail-without-innermost.is_superselector_of(marker.tail-without-innermost)
      else
        return lhs.tail-without-innermost.is_superselector_of(marker.tail-without-innermost)
    */
    if (lhs->combinator() != Complex_Selector::ANCESTOR_OF)
    {
      if (marker->combinator() == Complex_Selector::ANCESTOR_OF)
      { return false; }
      if (!(lhs->combinator() == Complex_Selector::PRECEDES ? marker->combinator() != Complex_Selector::PARENT_OF : lhs->combinator() == marker->combinator()))
      { return false; }
      lhs->clear_innermost();
      rhs->clear_innermost();
      bool is_sup = lhs->is_superselector_of(rhs);
      lhs->set_innermost(l_innermost);
      rhs->set_innermost(r_innermost);
      return is_sup;
    }
    else if (marker->combinator() != Complex_Selector::ANCESTOR_OF)
    {
      if (marker->combinator() != Complex_Selector::PARENT_OF)
      { return false; }
      lhs->clear_innermost();
      rhs->clear_innermost();
      bool is_sup = lhs->is_superselector_of(rhs);
      lhs->set_innermost(l_innermost);
      rhs->set_innermost(r_innermost);
      return is_sup;
    }
    else
    {
      lhs->clear_innermost();
      rhs->clear_innermost();
      bool is_sup = lhs->is_superselector_of(rhs);
      lhs->set_innermost(l_innermost);
      rhs->set_innermost(r_innermost);
      return is_sup;
    }
    // catch-all
    return false;
  }

  size_t Complex_Selector::length()
  {
    // TODO: make this iterative
    if (!tail()) return 1;
    return 1 + tail()->length();
  }

  Compound_Selector* Complex_Selector::base()
  {
    if (!tail()) return head();
    else return tail()->base();
  }

  Complex_Selector* Complex_Selector::context(Context& ctx)
  {
    if (!tail()) return 0;
    if (!head()) return tail()->context(ctx);
    return new (ctx.mem) Complex_Selector(path(), line(), combinator(), head(), tail()->context(ctx));
  }

  Complex_Selector* Complex_Selector::innermost()
  {
    if (!tail()) return this;
    else         return tail()->innermost();
  }

  void Complex_Selector::clear_innermost()
  {
    if (!tail() || tail()->length() == 1) tail(0);
    else                                  tail()->clear_innermost();
  }

  void Complex_Selector::set_innermost(Complex_Selector* val)
  {
    if (!tail()) tail(val);
    else         tail()->set_innermost(val);
  }

  Selector_Placeholder* Selector::find_placeholder()
  {
    return 0;
  }

  Selector_Placeholder* Selector_List::find_placeholder()
  {
    if (has_placeholder()) {
      for (size_t i = 0, L = length(); i < L; ++i) {
        if ((*this)[i]->has_placeholder()) return (*this)[i]->find_placeholder();
      }
    }
    return 0;
  }

  Selector_Placeholder* Complex_Selector::find_placeholder()
  {
    if (has_placeholder()) {
      if (head() && head()->has_placeholder()) return head()->find_placeholder();
      else if (tail() && tail()->has_placeholder()) return tail()->find_placeholder();
    }
    return 0;
  }

  Selector_Placeholder* Compound_Selector::find_placeholder()
  {
    if (has_placeholder()) {
      for (size_t i = 0, L = length(); i < L; ++i) {
        if ((*this)[i]->has_placeholder()) return (*this)[i]->find_placeholder();
      }
      // return this;
    }
    return 0;
  }

  Selector_Placeholder* Selector_Placeholder::find_placeholder()
  {
    return this;
  }

}