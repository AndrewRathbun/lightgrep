/*
  liblightgrep: not the worst forensics regexp engine
  Copyright (C) 2013, Lightbox Technologies, Inc

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <set>
#include <vector>

#include "sparseset.h"
#include "vm_interface.h"
#include "byteset.h"
#include "thread.h"

class Vm: public VmInterface {
public:

  typedef std::vector<Thread> ThreadList;

  Vm();
  Vm(ProgramPtr prog);

  // numCheckedStates should be equal to the number + 1 for the reserved bit
  void init(ProgramPtr prog);

  virtual void startsWith(const byte* const beg, const byte* const end, const uint64_t startOffset, HitCallback hitFn, void* userData);
  virtual bool search(const byte* const beg, const byte* const end, const uint64_t startOffset, HitCallback hitFn, void* userData);
  virtual void closeOut(HitCallback hitFn, void* userData);
  virtual void reset();

  #ifdef LBT_TRACE_ENABLED
  void setDebugRange(uint64_t beg, uint64_t end) {
    BeginDebug = beg;
    EndDebug = end;
  }
  #endif

  bool execute(Thread* t, const byte* const cur);
  bool execute(ThreadList::iterator t, const byte* const cur);

  bool executeEpsilon(Thread* t, uint64_t offset);
  bool executeEpsilon(ThreadList::iterator t, uint64_t offset);

  void executeFrame(const byte* const cur, uint64_t offset, HitCallback hitFn, void* userData);
  void cleanup();

  const ThreadList& first() const { return First; }
  const ThreadList& active() const { return Active; }
  const ThreadList& next() const { return Next; }

  Thread& add(const Thread& t) {
    Active.push_back(t);
    return Active.back();
  }

  uint32_t numActive() const { return Active.size(); }
  uint32_t numNext() const { return Next.size(); }

private:
  void _markSeen(const uint32_t label);
  void _markLive(const uint32_t label);
  bool _liveCheck(const uint64_t start, const uint32_t label);

  bool _execute(const Instruction* const base, ThreadList::iterator t, const byte* const cur);

  template <uint32_t X>
  bool _executeEpsilon(const Instruction* const base, ThreadList::iterator t, const uint64_t offset);

  template <uint32_t X>
  bool _executeEpSequence(const Instruction* const base, ThreadList::iterator t, const uint64_t offset);

  void _executeThread(const Instruction* const base, ThreadList::iterator t, const byte* const cur, const uint64_t offset);
  void _executeFrame(const ByteSet& first, ThreadList::iterator t, const Instruction* const base, const byte* const cur, const uint64_t offset);
  void _cleanup();

  #ifdef LBT_TRACE_ENABLED
  void open_init_epsilon_json(std::ostream& out);
  void close_init_epsilon_json(std::ostream& out) const;
  void open_frame_json(std::ostream& out, uint64_t offset, const byte* const cur);
  void close_frame_json(std::ostream& out, uint64_t offset) const;
  void pre_run_thread_json(std::ostream& out, uint64_t offset, const Thread& t,
                           const Instruction* const base);
  void post_run_thread_json(std::ostream& out, uint64_t offset, const Thread& t,
                            const Instruction* const base);
  void thread_json(std::ostream& out, const Thread& t,
                   const Instruction* const base, byte state);

  bool first_thread_json;
  std::set<uint64_t> new_thread_json;

  uint64_t BeginDebug, EndDebug;
  uint64_t NextId;
  #endif

  uint64_t MaxMatches;

  ProgramPtr Prog;
  const Instruction* ProgEnd;

  ThreadList First,
             Active,
             Next;

  bool SeenNoLabel;
  SparseSet Seen;

  bool LiveNoLabel;
  SparseSet Live;

  std::vector<uint64_t> MatchEnds;
  uint64_t MatchEndsMax;

  SparseSet CheckLabels;

  HitCallback CurHitFn;
  void* UserData;
};
