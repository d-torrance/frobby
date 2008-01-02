#include "stdinc.h"
#include "SliceStrategy.h"

#include "Slice.h"
#include "Term.h"
#include "Ideal.h"
#include "TermTranslator.h"
#include <vector>
#include "Projection.h"
#include "SliceAlgorithm.h"
#include "TermGrader.h"

SliceStrategy::~SliceStrategy() {
}

void SliceStrategy::initialize(const Slice& slice) {
}

void SliceStrategy::startingContent(const Slice& slice) {
}

void SliceStrategy::endingContent() {
}

void SliceStrategy::simplify(Slice& slice) {
  slice.simplify();
}

void SliceStrategy::getPivot(Term& pivot, const Slice& slice) {
  ASSERT(false);
  fputs("ERROR: SliceStrategy::getPivot called but not defined.\n", stderr);
  exit(1);
}

size_t SliceStrategy::getLabelSplitVariable(const Slice& slice) {
  ASSERT(false);
  fputs
    ("ERROR: SliceStrategy::getLabelSplitVariable called but not defined.\n",
     stderr);
  exit(1);
}

class DecomSliceStrategy : public SliceStrategy {
public:
  DecomSliceStrategy(TermConsumer* consumer):
    _consumer(consumer) {
  }

  virtual ~DecomSliceStrategy() {
    ASSERT(_independenceSplits.empty());
    delete _consumer;
  }

  virtual void consume(const Term& term) {
    ASSERT(getCurrentConsumer() != 0);

    getCurrentConsumer()->consume(term);
  }

  virtual void doingIndependenceSplit(const Slice& slice,
				      Ideal* mixedProjectionSubtract) {
    _independenceSplits.push_back
      (new IndependenceSplit(slice,
			     mixedProjectionSubtract,
			     getCurrentConsumer()));
  }
 
  virtual void doingIndependentPart(const Projection& projection, bool last) {
    getCurrentSplit()->setCurrentPart(projection, last);
  }

  virtual bool doneWithIndependentPart() {
    return getCurrentSplit()->doneWithPart();
  }

  virtual void doneWithIndependenceSplit() {
    delete getCurrentSplit();
    _independenceSplits.pop_back();
  }

private:
  class IndependenceSplit : public TermConsumer {
  public:
    IndependenceSplit(const Slice& slice,
		      Ideal* mixedProjectionSubtract,
		      TermConsumer* consumer):
      _partialTerm(slice.getMultiply()),
      _mixedProjectionSubtract(mixedProjectionSubtract),
      _consumer(consumer),
      _lastPartProjection(0) {
      ASSERT(_consumer != 0);
    }

    ~IndependenceSplit() {
    }

    virtual void consume(const Term& term) {
      if (_lastPartProjection != 0) {
	_lastPartProjection->inverseProject(_partialTerm, term);
	generateDecom();
      } else
	_parts.back().decom.insert(term);
    }

    // Must set each part exactly once, and must call doneWithPart
    // after having called setCurrentPart().
    bool setCurrentPart(const Projection& projection, bool last) {
      ASSERT(_lastPartProjection == 0);

      if (last)
	_lastPartProjection = &projection;
      else {
	if (_parts.empty()) {
	  // We reserve space to ensure that no reallocation will
	  // happen. Each part has at least two variables, and the
	  // last part is not stored.
	  _parts.reserve(_partialTerm.getVarCount() / 2 - 1);
	}
	_parts.resize(_parts.size() + 1);
	_parts.back().projection = &projection;
	_parts.back().decom.clearAndSetVarCount(projection.getRangeVarCount());
      }

      return true;
    }

    bool doneWithPart() {
      if (_lastPartProjection != 0)
	return true;

      bool hasDecom = (_parts.back().decom.getGeneratorCount() != 0);
      return hasDecom;
    }

  private:
    void generateDecom() {
      if (_parts.empty())
	outputDecom();
      else
	generateDecom(_parts.size() - 1);
    }

    void generateDecom(size_t part) {
      ASSERT(part < _parts.size());
      const Part& p = _parts[part];

      Ideal::const_iterator stop = p.decom.end();
      for (Ideal::const_iterator it = p.decom.begin(); it != stop; ++it) {
	p.projection->inverseProject(_partialTerm, *it);
	if (part == 0)
	  outputDecom();
	else
	  generateDecom(part - 1);
      }
    }

    void outputDecom() {
      ASSERT(_consumer != 0);
      if (_mixedProjectionSubtract == 0 ||
	  !_mixedProjectionSubtract->contains(_partialTerm))
	_consumer->consume(_partialTerm);
    }

    struct Part {
      const Projection* projection;
      Ideal decom;
    };
    vector<Part> _parts;

    Term _partialTerm;
    Ideal* _mixedProjectionSubtract;

    TermConsumer* _consumer;
    const Projection* _lastPartProjection;
  };

  IndependenceSplit* getCurrentSplit() {
    ASSERT(!_independenceSplits.empty());
    ASSERT(_independenceSplits.back() != 0);
    return _independenceSplits.back();
  }

  TermConsumer* getCurrentConsumer() {
    if (_independenceSplits.empty())
      return _consumer;
    else
      return getCurrentSplit();
  }
  
  vector<IndependenceSplit*> _independenceSplits;
  TermConsumer* _consumer;
};


class LabelSliceStrategy : public DecomSliceStrategy {
public:
  LabelSliceStrategy(TermConsumer* consumer):
    DecomSliceStrategy(consumer) {
  }

  SplitType getSplitType(const Slice& slice) {
    return LabelSplit;
  }

  size_t getLabelSplitVariable(const Slice& slice) {
    Term co(slice.getVarCount());

    // TODO: This is duplicate code from PivotSliceStrategy. Factor
    // out into Slice.
    for (Ideal::const_iterator it = slice.getIdeal().begin();
	 it != slice.getIdeal().end(); ++it) {
      for (size_t var = 0; var < slice.getVarCount(); ++var)
	if ((*it)[var] > 0)
	  ++co[var];
    }

    return co.getFirstMaxExponent();
  }
}; 

class PivotSliceStrategy : public DecomSliceStrategy {
public:
  enum Type {
    Min,
    Mid,
    Max
  };

  PivotSliceStrategy(Type type, TermConsumer* consumer):
    DecomSliceStrategy(consumer),
    _type(type) {
  }
 
  SplitType getSplitType(const Slice& slice) {
    return PivotSplit;
  }

  void getPivot(Term& pivot, const Slice& slice) {
    const Term& lcm = slice.getLcm();

    Term co(slice.getVarCount());

    for (Ideal::const_iterator it = slice.getIdeal().begin();
	 it != slice.getIdeal().end(); ++it) {
      for (size_t var = 0; var < slice.getVarCount(); ++var)
	if ((*it)[var] > 0)
	  ++co[var];
    }

    size_t maxOffset;
    do {
      maxOffset = co.getFirstMaxExponent();
      co[maxOffset] = 0;
    } while (lcm[maxOffset] <= 1);

    pivot.setToIdentity();
    Exponent& e = pivot[maxOffset];
    if (_type == Min)
      e = 1;
    else if (_type == Mid)
      e = lcm[maxOffset] / 2;
    else {
      ASSERT(_type == Max);
      e = lcm[maxOffset] - 1;
    }
  }

private:
  Type _type;
};

SliceStrategy* SliceStrategy::newDecomStrategy(const string& name,
											   TermConsumer* consumer) {
  if (name == "label")
    return new LabelSliceStrategy(consumer);
  else if (name == "minart")
    return new PivotSliceStrategy(PivotSliceStrategy::Min, consumer);
  else if (name == "midart")
    return new PivotSliceStrategy(PivotSliceStrategy::Mid, consumer);
  else if (name == "maxart")
    return new PivotSliceStrategy(PivotSliceStrategy::Max, consumer);

  fprintf(stderr, "ERROR: Unknown split strategy \"%s\".\n", name.c_str());
  exit(1);
}

// A decorator (pattern) for a SliceStrategy that does nothing. The
// purpose of this class is to act as a convenient base class for
// other decorators.
class DecoratorSliceStrategy : public SliceStrategy {
public:
  DecoratorSliceStrategy(SliceStrategy* strategy):
    _strategy(strategy) {
    ASSERT(strategy != 0);
  }

  virtual ~DecoratorSliceStrategy() {
    delete _strategy;
  }

  virtual void initialize(const Slice& slice) {
    _strategy->initialize(slice);
  }

  virtual void doingIndependenceSplit(const Slice& slice,
				      Ideal* mixedProjectionSubtract) {
    _strategy->doingIndependenceSplit
      (slice, mixedProjectionSubtract);
  }

  virtual void doingIndependentPart(const Projection& projection, bool last) {
    _strategy->doingIndependentPart(projection, last);
  }

  virtual bool doneWithIndependentPart() {
    return _strategy->doneWithIndependentPart();
  }

  virtual void doneWithIndependenceSplit() {
    _strategy->doneWithIndependenceSplit();
  }

  virtual void startingContent(const Slice& slice) {
    _strategy->startingContent(slice);
  }

  virtual void endingContent() {
    _strategy->endingContent();
  }

  virtual void simplify(Slice& slice) {
    _strategy->simplify(slice);
  }

  virtual SplitType getSplitType(const Slice& slice) {
    return _strategy->getSplitType(slice);
  }

  virtual void getPivot(Term& pivot, const Slice& slice) {
    _strategy->getPivot(pivot, slice);
  }

  virtual size_t getLabelSplitVariable(const Slice& slice) {
    return _strategy->getLabelSplitVariable(slice);
  }

  virtual void consume(const Term& term) {
    _strategy->consume(term);
  }

private:
  SliceStrategy* _strategy;
};

class FrobeniusIndependenceSplit : public TermConsumer {
public:
  FrobeniusIndependenceSplit(const TermGrader& grader):
    _grader(grader),
    _bound(grader.getVarCount()),
    _toBeat(-1),
    _improved(true),
    _partValue(-1), 
    _partProjection(&_projection) {
    _projection.setToIdentity(grader.getVarCount());
    _outerPartProjection = _projection;
  }

  FrobeniusIndependenceSplit(const Projection& projection,
							 const Slice& slice,
							 const mpz_class& toBeat,
							 const TermGrader& grader):
    _grader(grader),
    _bound(slice.getVarCount()),
    _toBeat(toBeat),
    _improved(true),
    _projection(projection) {
    getUpperBound(slice, _bound);
  }

  virtual void consume(const Term& term) {
    static mpz_class newDegree;
    getDegree(term, _outerPartProjection, newDegree);

    if (newDegree > _partValue) {
      _partProjection->inverseProject(_bound, term);
      _partValue = newDegree;
      _improved = true;
    }
  }

  const Term& getBound() const {
    return _bound;
  }

  bool hasImprovement() const {
    return _improved;
  }

  const mpz_class& getCurrentPartValue() const {
    return _partValue;
  }

  void getBoundDegree(mpz_class& degree) {
    getDegree(_bound, _projection, degree);
  }

  void setCurrentPart(const Projection& projection) {
    if (!_improved)
      return;

    _improved = false;
    _partProjection = &projection;

    updateOuterPartProjection();

    Term zero(_partProjection->getRangeVarCount());
    _partProjection->inverseProject(_bound, zero);
    getDegree(zero, _outerPartProjection, _partValue);

    if (_toBeat == -1)
      _partValue = -1;
    else {
      static mpz_class tmp;
      getDegree(_bound, _projection, tmp);
      _partValue = _toBeat - (tmp - _partValue);
    }
  }

  bool doneWithPart() {
    return _improved;
  }

  const Projection& getCurrentPartProjection() {
    return _outerPartProjection;
  }

  void simplify(Slice& slice) {
    if (_partValue == -1) {
      slice.simplify();
      return;
    }

	do
	  slice.simplify();
	while (basicBoundSimplify(slice));
  }

private:
  // This has not turned out to work well.
  bool involvedBoundSimplify(Slice& slice) {
    if (slice.getIdeal().getGeneratorCount() == 0)
      return false;

    mpz_class degree;

    Term colon(slice.getVarCount());
    Term lcm(slice.getVarCount());
    for (size_t var = 0; var < slice.getVarCount(); ++var) {
      lcm.setToIdentity();

      const Ideal& ideal = slice.getIdeal();
      for (Ideal::const_iterator it = ideal.begin(); it != ideal.end(); ++it)
		if ((*it)[var] <= 1)
		  lcm.lcm(lcm, *it);

      adjustToBound(slice, lcm);
      getDegree(lcm, _outerPartProjection, degree);
      if (degree <= _partValue)
		colon[var] = 1;
    }

    if (colon.isIdentity())
      return false;

    slice.innerSlice(colon);
    return true;
  }

  bool canExclude(size_t var,
				  const mpz_class& upperBoundDegree,
				  Exponent oldUpper,
				  Exponent newUpper) {
	size_t outerVar = _outerPartProjection.inverseProjectVar(var);
	static mpz_class difference;
	static mpz_class degreeLess;
	difference =
	  _grader.getGrade(outerVar, oldUpper) -
	  _grader.getGrade(outerVar, newUpper);

	degreeLess = upperBoundDegree - difference;
	return degreeLess <= _partValue;
  }

  Exponent improveLowerBound(size_t var,
							 const mpz_class& upperBoundDegree,
							 const Term& upperBound,
							 const Term& lowerBound) {
	if (upperBound[var] == lowerBound[var])
	  return 0;

	// Binary search, with an initial test at the lowest end.
	Exponent low = 0;
	Exponent high = upperBound[var] - lowerBound[var];

	while (low < high) {
	  Exponent mid;
	  if (low < high - low)
		mid = 2 * low;
	  else {
		// This way of expressing (low + high) / 2 avoids the
		// possibility of low + high causing an overflow.
		mid = low + (high - low) / 2;
	  }
	  
	  if (canExclude(var, upperBoundDegree,
					 upperBound[var], lowerBound[var] + mid))
		low = mid + 1;
	  else
		high = mid;
	}
	ASSERT(low == high);

	return low;
  }

  bool basicBoundSimplify(Slice& slice) {
    if (slice.getIdeal().getGeneratorCount() == 0)
      return false;

    Term bound(slice.getVarCount());
    mpz_class degree;
    mpz_class degreeLess;
    mpz_class difference;
    
    getUpperBound(slice, bound);
    getDegree(bound, _outerPartProjection, degree);

    if (degree <= _partValue) {
      slice.clear();
      return false;
    }

    Term colon(slice.getVarCount());
    for (size_t var = 0; var < slice.getVarCount(); ++var) {
	  colon[var] =
		improveLowerBound(var, degree, bound, slice.getMultiply());
	}

    if (colon.isIdentity())
      return false;

    slice.innerSlice(colon);
    return true;
  }

  void updateOuterPartProjection() {
    vector<size_t> inverses;
    for (size_t var = 0; var < _partProjection->getRangeVarCount(); ++var) {
      size_t middleVar = _partProjection->inverseProjectVar(var);
      size_t outerVar = _projection.inverseProjectVar(middleVar);
      
      inverses.push_back(outerVar);
    }

    _outerPartProjection.reset(inverses);
  }

  void getUpperBound(const Slice& slice,
					 Term& bound) {
    ASSERT(bound.getVarCount() == slice.getVarCount());
    bound = slice.getLcm();
    adjustToBound(slice, bound);
  }

  void adjustToBound(const Slice& slice, Term& bound) {
    ASSERT(bound.getVarCount() == slice.getVarCount());

    bound.product(bound, slice.getMultiply());

    for (size_t var = 0; var < bound.getVarCount(); ++var) {
      ASSERT(bound[var] > 0);
      --bound[var];
    }

    for (size_t var = 0; var < bound.getVarCount(); ++var)
      if (bound[var] == _grader.getMaxExponent(var) &&
		  slice.getMultiply()[var] < bound[var])
		--bound[var];
  }

  void getDegree(const Term& term,
				 const Projection& projection,
				 mpz_class& degree) {
    _grader.getDegree(term, projection, degree);
  }

  const TermGrader& _grader;

  Term _bound;
  mpz_class _toBeat;
  bool _improved;

  mpz_class _partValue;
  const Projection* _partProjection;
  Projection _outerPartProjection;

  Projection _projection;
};

// Ignores subtraction of mixed generators when doing independence
// splits. This is no problem right now, but it would become a problem
// if SliceAlgorithm at a later point gets the (externally exposed)
// capability to compute slices in general, rather than a complete
// decomposition.
class FrobeniusSliceStrategy : public DecoratorSliceStrategy {
public:
  FrobeniusSliceStrategy(SliceStrategy* strategy,
			 TermConsumer* consumer,
			 TermGrader& grader):
    DecoratorSliceStrategy(strategy),
    _consumer(consumer),
    _grader(grader) {
    ASSERT(_consumer != 0);

    _independenceSplits.push_back
      (new FrobeniusIndependenceSplit(_grader));
  }

  virtual ~FrobeniusSliceStrategy() {
    ASSERT(_consumer != 0);
    _consumer->consume(getCurrentSplit()->getBound());

    delete getCurrentSplit();
    _independenceSplits.pop_back();
    ASSERT(_independenceSplits.empty());
  }

  virtual void initialize(const Slice& slice) {
    // The purpose of this is to initialize the bound so that it can
    // be used right away, instead of waiting for the slice algorithm
    // to produce some output first.
    Term msm;
    if (computeSingleMSM(slice, msm))
      consume(msm);
  }

  virtual void simplify(Slice& slice) {
    getCurrentSplit()->simplify(slice);
  }

  virtual void consume(const Term& term) {
    getCurrentSplit()->consume(term);
  }

  virtual void doingIndependenceSplit(const Slice& slice,
				      Ideal* mixedProjectionSubtract) {
    _independenceSplits.push_back
      (new FrobeniusIndependenceSplit
       (getCurrentSplit()->getCurrentPartProjection(),
	slice,
	getCurrentSplit()->getCurrentPartValue(),
	_grader));
  }

  virtual void doingIndependentPart(const Projection& projection, bool last) {
    getCurrentSplit()->setCurrentPart(projection);
  }

  virtual bool doneWithIndependentPart() {
    return getCurrentSplit()->doneWithPart();
  }

  virtual void doneWithIndependenceSplit() {
    FrobeniusIndependenceSplit* oldSplit = getCurrentSplit();
    _independenceSplits.pop_back();

    if (oldSplit->hasImprovement())
      consume(oldSplit->getBound());
    delete oldSplit;
  }

private:
  FrobeniusIndependenceSplit* getCurrentSplit() {
    ASSERT(!_independenceSplits.empty());
    return _independenceSplits.back();
  }

  TermConsumer* _consumer;
  vector<FrobeniusIndependenceSplit*> _independenceSplits;
  const TermGrader& _grader;
};

SliceStrategy* SliceStrategy::
newFrobeniusStrategy(const string& name,
		     TermConsumer* consumer,
		     TermGrader& grader) {
  SliceStrategy* strategy = newDecomStrategy(name, 0);
  return new FrobeniusSliceStrategy(strategy, consumer, grader);
}

class StatisticsSliceStrategy : public DecoratorSliceStrategy {
public:
  StatisticsSliceStrategy(SliceStrategy* strategy):
    DecoratorSliceStrategy(strategy),
    _level (0),
    _consumeCount(0),
    _independenceSplitCount(0) {
  }

  ~StatisticsSliceStrategy() {
    fputs("**** Statistics\n", stderr);
    size_t sum = 0;
    size_t baseSum = 0;
    for (size_t l = 0; l < _calls.size(); ++l) {
      sum += _calls[l];
      baseSum += _baseCases[l];
      if (false) {
	fprintf(stderr,
		"Level %lu had %lu calls of which %lu were base cases.\n",
		(unsigned long)l + 1,
		(unsigned long)_calls[l],
		(unsigned long)_baseCases[l]);
      }
    }

    double baseRatio = double(baseSum) / sum;
    fprintf(stderr,
	    "* recursive levels:    %lu\n"
	    "* recursive calls:     %lu\n"
	    "* terms consumed:      %lu\n" 
	    "* base case calls:     %lu (%f)\n"
	    "* independence splits: %lu\n"
	    "****\n",
	    (unsigned long)_calls.size(),
	    (unsigned long)sum,
	    (unsigned long)_consumeCount,
	    (unsigned long)baseSum,
	    baseRatio,
	    (unsigned long)_independenceSplitCount);
  }

  virtual void doingIndependenceSplit(const Slice& slice,
				      Ideal* mixedProjectionSubtract) {
    ++_independenceSplitCount;
    DecoratorSliceStrategy::doingIndependenceSplit(slice,
						   mixedProjectionSubtract);
  }

  void startingContent(const Slice& slice) {
    if (_level > 0)
      _isBaseCase[_level - 1] = false;

    if (_calls.size() == _level) {
      _calls.push_back(0);
      _baseCases.push_back(0);
      _isBaseCase.push_back(true);
    }

    _isBaseCase[_level] = true;
    ++_calls[_level];
    ++_level;

    DecoratorSliceStrategy::startingContent(slice);
  }

  void endingContent() {
    --_level;
    if (_isBaseCase[_level])
      ++_baseCases[_level];

    DecoratorSliceStrategy::endingContent();
  }

  void consume(const Term& term) {
    ++_consumeCount;

    DecoratorSliceStrategy::consume(term);
  }

private:
  size_t _level;
  vector<size_t> _calls;
  vector<size_t> _baseCases;
  vector<size_t> _isBaseCase;
  size_t _consumeCount;
  size_t _independenceSplitCount;
};

SliceStrategy* SliceStrategy::addStatistics(SliceStrategy* strategy) {
  return new StatisticsSliceStrategy(strategy);
}

class DebugSliceStrategy : public DecoratorSliceStrategy {
 public:
  DebugSliceStrategy(SliceStrategy* strategy):
    DecoratorSliceStrategy(strategy),
    _level(0) {
  }
  
  virtual void doingIndependenceSplit(const Slice& slice,
				      Ideal* mixedProjectionSubtract) {
    fprintf(stderr, "DEBUG %lu: doing independence split.\n",
	    (unsigned long)_level);
    fflush(stderr);
    DecoratorSliceStrategy::doingIndependenceSplit
      (slice, mixedProjectionSubtract);
  }

  virtual void doingIndependentPart(const Projection& projection, bool last) {
    fprintf(stderr, "DEBUG %lu: doing independent part\n",
	    (unsigned long)_level);
    if (last)
      fputs(" (last)", stderr);
    fputs(".\n", stderr);
    fflush(stderr);
    DecoratorSliceStrategy::doingIndependentPart(projection, last);
  }

  virtual bool doneWithIndependentPart() {
    fprintf(stderr, "DEBUG %lu: done with that independent part.\n",
	    (unsigned long)_level);
    fflush(stderr);
    
    return DecoratorSliceStrategy::doneWithIndependentPart();
  }
  
  virtual void doneWithIndependenceSplit() {
    fprintf(stderr, "DEBUG %lu: done with independence split.\n",
	    (unsigned long)_level);
    fflush(stderr);
    DecoratorSliceStrategy::doneWithIndependenceSplit();
  }

  void startingContent(const Slice& slice) {
    ++_level;
    fprintf(stderr, "DEBUG %lu: computing content of the following slice.\n",
	    (unsigned long)_level);
    slice.print(stderr);
    fflush(stderr);

    DecoratorSliceStrategy::startingContent(slice);
  }

  void endingContent() {
    fprintf(stderr, "DEBUG %lu: done computing content of that slice.\n",
	    (unsigned long)_level);
    fflush(stderr);
    --_level;

    DecoratorSliceStrategy::endingContent();
  }

  void getPivot(Term& pivot, const Slice& slice) {
    DecoratorSliceStrategy::getPivot(pivot, slice);
    fprintf(stderr, "DEBUG %lu: performing pivot split on ",
	    (unsigned long)_level);
    pivot.print(stderr);
    fputs(".\n", stderr);
    fflush(stderr);
  }

  size_t getLabelSplitVariable(const Slice& slice) {
    size_t var = DecoratorSliceStrategy::getLabelSplitVariable(slice);
    fprintf(stderr, "DEBUG %lu: performing label split on var %lu.\n",
	    (unsigned long)_level,
	    (unsigned long)var);
    fflush(stderr);
    return var;
  }

  void consume(const Term& term) {
    fprintf(stderr, "DEBUG %lu: Writing ", (unsigned long)_level);
    term.print(stderr);
    fputs(" to output.\n", stderr);
    fflush(stderr);

    DecoratorSliceStrategy::consume(term);
  }
  
private:
  size_t _level;
};

SliceStrategy* SliceStrategy::addDebugOutput(SliceStrategy* strategy) {
  return new DebugSliceStrategy(strategy);
}
