#include "parser.h"

#include "states.h"

#include <iostream>

std::ostream& operator<<(std::ostream& out, const VList& list) {
  out << '{';
  for (VList::const_iterator it(list.begin()); it != list.end(); ++it) {
    if (it != list.begin()) {
      out << ", ";
    }
    out << *it;
  }
  out << '}';
  return out;
}

std::ostream& operator<<(std::ostream& out, const Fragment& f) {
  out << "in " << f.InList << ", out " << f.OutList;
  return out;
}

void Fragment::add(DynamicFSM::vertex_descriptor v, VList& list) {
  if (std::find(list.begin(), list.end(), v) == list.end()) {
    list.push_back(v);
  }
}

void Fragment::merge(const Fragment& f) {
  // std::cout << "merge in " << InList << ", " << f.InList << ", out " << OutList << ", " << f.OutList << std::endl;
  mergeLists(InList, f.InList);
  mergeLists(OutList, f.OutList);
  // std::cout << "merged in " << InList << ", out " << OutList << std::endl;
}

void Fragment::mergeLists(VList& l1, const VList& l2) {
  for (VList::const_iterator it(l2.begin()); it != l2.end(); ++it) {
    add(*it, l1);
  }
}

Parser::Parser():
  CurLabel(0)
{
  reset();
}

void Parser::reset() {
  IsGood = false;
  Fsm.reset(new DynamicFSM(1));
  VList first;
  first.push_back(0);
  Stack.push(Fragment(first, Node(), first));
}

void Parser::patch(const VList& sources, const VList& targets) {
  // std::cout << "patch " << sources << "->" << targets << std::endl;
  for (VList::const_iterator srcIt(sources.begin()); srcIt != sources.end(); ++srcIt) {
    for (VList::const_iterator targetIt(targets.begin()); targetIt != targets.end(); ++targetIt) {
      // std::cout << "Making edge (" << *srcIt << ", " << *targetIt << ")" << std::endl;
      boost::add_edge(*srcIt, *targetIt, *Fsm);
    }
  }
}

Fragment Parser::patch(const Fragment& first, const Fragment& second, const Node& n) {
  Fragment ret;
  ret.N = n;
  // std::cout << "patching states" << std::endl;
  patch(first.OutList, second.InList);
  ret.InList = first.InList;
  ret.OutList = second.OutList;
  if (first.Skippable) {
    Fragment::mergeLists(ret.InList, second.InList);
  }
  if (second.Skippable) {
    Fragment::mergeLists(ret.OutList, first.OutList);
  }
  return ret;
}

void Parser::addAtom(const Node&) {
  // don't really have to do anything here
}

void Parser::alternate(const Node& n) {
  Fragment second = Stack.top();
  Stack.pop();
  Fragment first = Stack.top();
  Stack.pop();
  // std::cout << "alternation, in " << second.InList << ", " << first.InList << ", out " << second.OutList << ", " << first.OutList << std::endl;
  first.merge(second);
  first.N = n;
  first.Skippable = first.Skippable || second.Skippable;
  Stack.push(first);
}

void Parser::concatenate(const Node& n) {
  Fragment second = Stack.top();
  Stack.pop();
  Fragment first = Stack.top();
  Stack.pop();
  Stack.push(patch(first, second, n));
}

void Parser::literal(const Node& n) {
  DynamicFSM& g(*Fsm);
  DynamicFSM::vertex_descriptor v = boost::add_vertex(g);
  g[v].reset(new LitState(n.Val));
  VList in, out;
  in.push_back(v);
  out.push_back(v);
  Stack.push(Fragment(in, n, out));
}

void Parser::group(const Node&) {
  // don't really have to do anything here
}

void Parser::star(const Node& n) {
  plus(n);
  question(n);
}

void Parser::plus(const Node& n) {
  Fragment& repeat = Stack.top();
  repeat.N = n;
  patch(repeat.OutList, repeat.InList); // back-edges
}

void Parser::question(const Node&) {
  Fragment& optional = Stack.top();
  optional.Skippable = true;
}

void Parser::dot(const Node& n) {
  DynamicFSM::vertex_descriptor v = boost::add_vertex(*Fsm);
  (*Fsm)[v].reset(new RangeState(0, 255));
  VList in, out;
  in.push_back(v);
  out.push_back(v);
  Stack.push(Fragment(in, n, out));
}

void Parser::charClass(const Node& n, const std::string& lbl) {
  DynamicFSM::vertex_descriptor v = boost::add_vertex(*Fsm);
  uint32 num = 0;
  byte first = 0, last = 0;
  for (uint32 i = 0; i < 256; ++i) {
    if (n.Bits.test(i)) {
      if (!num) {
        first = i;
      }
      if (++num == n.Bits.count()) {
        last = i;
        break;
      }
    }
    else {
      num = 0;
    }
  }
  if (num == n.Bits.count()) {
    (*Fsm)[v].reset(new RangeState(first, last));
  }
  else {
    (*Fsm)[v].reset(new CharClassState(n.Bits, lbl));
  }
  VList in, out;
  in.push_back(v);
  out.push_back(v);
  Stack.push(Fragment(in, n, out));
}

void Parser::finish(const Node& n) {
  if (2 == Stack.size()) {
    Fragment path = Stack.top();
    Stack.pop();
    Fragment start = Stack.top();
    Stack.pop();
    Fragment final = patch(start, path, n);    
    for (VList::const_iterator it(final.OutList.begin()); it != final.OutList.end(); ++it) {
      // std::cout << "marking " << *it << " as a match" << std::endl;
      if (0 == *it) { // State 0 is not allowed to be a match state; i.e. 0-length REs are not allowed
        reset();
        return;
      }
      else {
        (*Fsm)[*it]->Label = CurLabel;
      }
    }
    // std::cout << "final is " << final << std::endl;
    IsGood = true;
  }
  else {
    reset();
    return;
    // THROW_RUNTIME_ERROR_WITH_OUTPUT("Final parse stack size should be 2, was " << Stack.size());
  }
}

void Parser::callback(const std::string& type, Node n) {
  // std::cout << type << std::endl;
  type.size();
  switch (n.Type) {
    case REGEXP:
      finish(n);
      break;
    case ALTERNATION:
      alternate(n);
      break;
    case CONCATENATION:
      concatenate(n);
      break;
    case GROUP:
      group(n);
      break;
    case STAR:
      star(n);
      break;
    case PLUS:
      plus(n);
      break;
    case QUESTION:
      question(n);
      break;
    case ATOM:
      addAtom(n);
      break;
    case DOT:
      dot(n);
      break;
    case CHAR_CLASS:
      charClass(n, type);
      break;
    case LITERAL:
      literal(n);
      break;
    default:
      break;
  }
  // std::cout << "Stack size is " << Stack.size() << std::endl;
  // if (Stack.size() > 0) {
  //   std::cout << "top is " << Stack.top() << std::endl;
  // }
}
