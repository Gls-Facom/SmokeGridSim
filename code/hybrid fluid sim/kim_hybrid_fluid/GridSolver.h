#ifndef __GridSolver_h
#define __GridSolver_h

#include "utils/Stopwatch.h"
#include "math/Vector3.h"
#include "PhysicsAnimation.h"
#include "FaceCenteredGrid.h"
#include "CellCenteredScalarGrid.h"
#include "GridBackwardEulerDiffusionSolver.h"
#include "GridFractionalSinglePhasePressureSolver.h"
#include "GridFractionalBoundaryConditionSolver.h"
#include "Collider.h"
#include "Constants.h"


namespace cg
{

  template <size_t D, typename real>
  class GridSolver : public PhysicsAnimation
  {
  public:
    using vec = Vector<real, D>;
    template <typename T> using Ref = Reference<T>;

    GridSolver(const Index<D>& size, const vec& spacing, const vec& origin)
    {
      _velocity = new FaceCenteredGrid<D, real>(size+2, spacing, origin-spacing);
      _density = new CellCenteredScalarGrid<D, real>(size+2, spacing, origin-spacing);
      _solverSize = Index2{ size.x,size.y };
      // use Adaptive SubTimeStepping
      this->setIsUsingFixedSubTimeSteps(false);
    }

    virtual ~GridSolver()
    {
      // do nothing
    }

    // Returns gravity for this solver.
    const auto& gravity() const { return _gravity; };

    // Sets the gravity for this solver.
    void setGravity(const vec& gravity) { _gravity = gravity; };

    // Returns the fluid viscosity.
    const auto& viscosityCoefficient() const { return _viscosityCoefficient; }

    // Sets the viscosity coefficient. Non-positive input will clamped to zero.
    void setViscosityCoefficient(real viscosity) { _viscosityCoefficient = math::max<real>(viscosity, 0.0f); }

    // Returns the CFL number from current velocity field for given time interval.
    real cfl(double timeInterval) const;

    // Returns the max allowed CFL number.
    real maxCfl() const { return _maxCfl; }

    // Sets the max allowed CFL number.
    void setMaxCfl(real newCfl) { _maxCfl = math::max(newCfl, math::Limits<real>::eps()); }

    // Returns the closed domain boundary flag.
    int closedDomainBoundaryFlag() const { return _closedDomainBoundaryFlag; }

    // Sets the closed domain boundary flag.
    void setClosedDomainBoundaryFlag(int flag);

    // Returns grid size.
    const auto& size() const { return _solverSize; }

    // Returns grid cell spacing.
    const auto& gridSpacing() const { return _velocity->gridSpacing(); }

    // Returns grid origin.
    const auto& gridOrigin() const { return _velocity->origin(); }

    // Returns the velocity field, represented by a FaceCenteredGrid
    const auto& velocity() const { return _velocity; }

    // Returns the density field, represented by a CellCenteredScalarGrid
    const auto& density() const { return _density; }

    const auto& collider() const { return _collider; }

    void setCollider(Collider<D, real>* collider);

    /*const auto& emitter() const;
     TODO
    void setEmitte(const GridEmitter* emitter);*/


  protected:
    // PhysicsAnimation virtual functions
    void initialize() override;

    void onAdvanceTimeStep(double timeInterval) override;

    size_t numberOfSubTimeSteps(double timeInterval) const override;

    // Called at the beginning of a time-step.
    virtual void onBeginAdvanceTimeStep(double timeInterval);

    // Called at the end of a time-step.
    virtual void onEndAdvanceTimeStep(double timeInterval);

    virtual void computeExternalForces(double timeInterval);

    virtual void computeViscosity(double timeInterval);

    virtual void computePressure(double timeInterval);

    virtual void computeAdvection(double timeInterval);

    void advectDensity(Index2 idx, double timeInterval);
   
    void advectVelocity(Index2 idx, double timeInterval);

    void computeSource(double timeInterval);

    void computeGravity(double timeInterval);
    
    void densityStep(double timeInterval);
    
    void velocityStep(double timeInterval);

    virtual ScalarField<D, real>* fluidSdf() const;

    void applyBoundaryCondition();

    void extrapolateIntoCollider(CellCenteredScalarGrid<D, real>& grid);

    ScalarField<D, real>* colliderSdf() const;

    VectorField<D, real>* colliderVelocityField() const;

  private:
    Index2 _solverSize{ 0 , 0};
    vec _gravity{ real(0.0f), real(-9.8f) };
    real _viscosityCoefficient{ 0.0f };
    real _maxCfl{ 5.0f };
    int _closedDomainBoundaryFlag{ constants::directionAll };

    Ref<FaceCenteredGrid<D, real>> _velocity;
    Ref<CellCenteredScalarGrid<D, real>> _density;
    Ref<Collider<D, real>> _collider;
    // grid Emitter TODO

    // Solvers
    GridBackwardEulerDiffusionSolver<D, real, false> _diffusionSolver;
    GridFractionalSinglePhasePressureSolver<D, real> _pressureSolver;
    GridFractionalBoundaryConditionSolver<D, real> _boundaryConditionSolver;
    // advectionSolver TODO

    void beginAdvanceTimeStep(double timeInterval);

    void endAdvanceTimeStep(double timeInterval);

    void updateCollider(double timeInterval);

    void updateEmitter(double timeInterval);

  }; // GridSolver<D, real>

  template<size_t D, typename real>
  inline real
    GridSolver<D, real>::cfl(double timeInterval) const
  {
    real maxVel = 0.0f;
    forEachIndex<D>(_velocity->size(), [&](const Index<D>& index) {
      auto v = _velocity->valueAtCellCenter(index) + timeInterval * _gravity;
      maxVel = math::max(maxVel, v.max());
      });

    return real(maxVel * timeInterval / _velocity->gridSpacing().min());
  }

  template<size_t D, typename real>
  inline void
    GridSolver<D, real>::setClosedDomainBoundaryFlag(int flag)
  {
    _closedDomainBoundaryFlag = flag;
    _boundaryConditionSolver.setClosedDomainBoundaryFlag(flag);
  }

  template<size_t D, typename real>
  inline void
    GridSolver<D, real>::setCollider(Collider<D, real>* collider)
  {
    _collider = collider;
  }

  template<size_t D, typename real>
  inline void
    GridSolver<D, real>::initialize()
  {
    updateCollider(0.0);

    updateEmitter(0.0);
  }

  template<size_t D, typename real>
  inline void
    GridSolver<D, real>::densityStep(double timeInterval)
  {
    computeSource(timeInterval);

    computeViscosity(timeInterval);//k=0, passthrough

    computeAdvection(timeInterval);
  }

  template<size_t D, typename real>
  inline void
    GridSolver<D, real>::velocityStep(double timeInterval)
  {
#ifdef _DEBUG
    // asserting min grid size
    assert(_velocity->size().min() > 0);
#endif // _DEBUG

    beginAdvanceTimeStep(timeInterval);

    //computeExternalForces(timeInterval);

    computeViscosity(timeInterval);//k=0, passthrough

    computePressure(timeInterval);

    computeAdvection(timeInterval);

    endAdvanceTimeStep(timeInterval);
  }

  template<size_t D, typename real>
  inline void
    GridSolver<D, real>::onAdvanceTimeStep(double timeInterval)
  {
#ifdef _DEBUG
    // asserting min grid size
    assert(_velocity->size().min() > 0);
#endif // _DEBUG

    beginAdvanceTimeStep(timeInterval);

    densityStep(timeInterval);

    velocityStep(timeInterval);

    endAdvanceTimeStep(timeInterval);
  }

  template<size_t D, typename real>
  inline size_t
    GridSolver<D, real>::numberOfSubTimeSteps(double timeInterval) const
  {
    auto _cfl = cfl(timeInterval);
    return static_cast<size_t>(math::max<real>(std::ceil(_cfl / _maxCfl), 1.0f));
  }

  template<size_t D, typename real>
  inline void
    GridSolver<D, real>::onBeginAdvanceTimeStep(double timeInterval)
  {
    // do nothing
  }

  template<size_t D, typename real>
  inline void
    GridSolver<D, real>::onEndAdvanceTimeStep(double timeInterval)
  {
    // do nothing
  }

  template<size_t D, typename real>
  inline void
    GridSolver<D, real>::computeExternalForces(double timeInterval)
  {
    computeGravity(timeInterval);
  }

  template<size_t D, typename real>
  inline void
    GridSolver<D, real>::computeViscosity(double timeInterval)
  {
    Stopwatch s;
    if (math::isPositive(_viscosityCoefficient))
    {
      s.start();
      _diffusionSolver.solve(
        _velocity,
        _viscosityCoefficient,
        timeInterval,
        _velocity,
        *colliderSdf(),
        *fluidSdf()
      );
      //debug("[INFO] Viscosity solver took %lld ms\n", s.time());

      applyBoundaryCondition();
    }
  }

  template<size_t D, typename real>
  inline void
    GridSolver<D, real>::computePressure(double timeInterval)
  {
    Stopwatch s;
    s.start();
    _pressureSolver.solve(
      _velocity,
      timeInterval,
      _velocity,
      *colliderSdf(),
      *fluidSdf(),
      *colliderVelocityField()
    );
    //debug("[INFO] Pressure solver took %lld ms\n", s.time());

    applyBoundaryCondition();
  }

  template<size_t D, typename real>
  inline void GridSolver<D, real>::advectVelocity(Index2 idx, double timeInterval)
  {
    //TODO
  }

  template<size_t D, typename real>
  inline void GridSolver<D, real>::advectDensity(Index2 idx, double timeInterval)
  {
    auto vel = vec(_velocity->velocityAt<0>(idx), _velocity->velocityAt<1>(idx));
    auto pos = _density->dataPosition(idx);
    auto bounds = _density->bounds();
    auto cellSize = _density->cellSize();

    auto minX = bounds.min().x + cellSize.x;
    auto maxX = bounds.max().x - cellSize.x;
    auto minY = bounds.min().y + cellSize.y;
    auto maxY = bounds.max().y - cellSize.y;

    //Stop backtracing at cell face
    auto newPos = pos - vel * timeInterval;
    newPos.x = newPos.x < minX ? minX : newPos.x;
    newPos.x = newPos.x > maxX ? maxX : newPos.x;
    newPos.y = newPos.y < minY ? minY : newPos.y;
    newPos.y = newPos.y > maxY ? maxY : newPos.y;

    _density->operator[](idx) = _density->sample(newPos);
  }

  template<size_t D, typename real>
  inline void
    GridSolver<D, real>::computeAdvection(double timeInterval)
  {
    auto N = size().x;
    Index2 index;
    for (index.y = 1; index.y <= N; ++index.y)
      for (index.x = 1; index.x <= N; ++index.x)
        advectDensity(index, timeInterval);
    
    applyBoundaryCondition();
  }

  template<size_t D, typename real>
  inline void GridSolver<D, real>::computeSource(double timeInterval)
  {    
    applyBoundaryCondition();
  }

  template<size_t D, typename real>
  inline void
    GridSolver<D, real>::computeGravity(double timeInterval)
  {
    if (this->_gravity.squaredNorm() > math::Limits<real>::eps())
    {
      std::array<int, D> sizes;
      sizes[0] = _velocity->iSize<0>().prod();
      sizes[1] = _velocity->iSize<1>().prod();
      if constexpr (D == 3)
        sizes[2] = _velocity->iSize<2>().prod();

      if (!math::isZero(_gravity.x))
      {
        for (int i = 0; i < sizes[0]; ++i)
          _velocity->velocityAt<0>(i) += timeInterval * _gravity.x;
      }

      if (!math::isZero(_gravity.y))
      {
        for (int i = 0; i < sizes[1]; ++i)
          _velocity->velocityAt<1>(i) += timeInterval * _gravity.y;
      }

      if constexpr (D == 3)
      {
        if (!math::isZero(_gravity.z))
        {
          for (int i = 0; i < sizes[2]; ++i)
            _velocity->velocityAt<2>(i) += timeInterval * _gravity.z;
        }
      }

      applyBoundaryCondition();
    }
  }

  template<size_t D, typename real>
  inline ScalarField<D, real>*
    GridSolver<D, real>::fluidSdf() const
  {
    return new ConstantScalarField<D, real>(math::Limits<real>::inf());
  }

  template<size_t D, typename real>
  inline void
    GridSolver<D, real>::applyBoundaryCondition()
  {
    /*auto depth = static_cast<unsigned int>(std::ceil(_maxCfl));
    _boundaryConditionSolver.constrainVelocity(_velocity, depth);*/
    auto N = size().x;
    for (int i = 1; i <= N; i++)
    {
      _density->operator[](Index2(0, i)) = _density->operator[](Index2(1, i));
      _density->operator[](Index2(N+1, i)) = _density->operator[](Index2(N, i));
      _density->operator[](Index2(i, 0)) = _density->operator[](Index2(i, 1));
      _density->operator[](Index2(i, N+1)) = _density->operator[](Index2(i, N));
    }
    _density->operator[](Index2(0, 0)) = .5f* (_density->operator[](Index2(1, 0))+ _density->operator[](Index2(0, 1)));
    _density->operator[](Index2(0, N+1)) = .5f * (_density->operator[](Index2(1, N+1)) + _density->operator[](Index2(0, N)));
    _density->operator[](Index2(N+1, 0)) = .5f * (_density->operator[](Index2(N, 0)) + _density->operator[](Index2(N+1, 1)));
    _density->operator[](Index2(N+1, N+1)) = .5f * (_density->operator[](Index2(N, N+1)) + _density->operator[](Index2(N+1, N)));
  }

  template<size_t D, typename real>
  inline void
    GridSolver<D, real>::extrapolateIntoCollider(CellCenteredScalarGrid<D, real>& grid)
  {
    GridData<D, char> marker;
    marker.resize(grid.dataSize());

    for (size_t i = 0; i < grid.length(); ++i)
    {
      auto index = grid.index(i);
      auto colliderSdfQuery = colliderSdf()->sample(grid.dataPosition(index));
      if (isInsideSdf(colliderSdfQuery))
        marker[i] = 0;
      else
        marker[i] = 1;
    }

    auto depth = static_cast<unsigned int>(std::ceil(_maxCfl));
    extrapolateToRegion(grid, marker, depth, grid);
  }

  template<size_t D, typename real>
  inline ScalarField<D, real>*
    GridSolver<D, real>::colliderSdf() const
  {
    return _boundaryConditionSolver.colliderSdf();
  }

  template<size_t D, typename real>
  inline VectorField<D, real>*
    GridSolver<D, real>::colliderVelocityField() const
  {
    return _boundaryConditionSolver.colliderVelocityField();
  }

  template<size_t D, typename real>
  inline void
    GridSolver<D, real>::beginAdvanceTimeStep(double timeInterval)
  {
    updateCollider(timeInterval);

    updateEmitter(timeInterval);

    _boundaryConditionSolver.updateCollider(
      _collider,
      _velocity->size(),
      _velocity->gridSpacing(),
      _velocity->origin()
    );

    applyBoundaryCondition();

    // Invoke callback
    onBeginAdvanceTimeStep(timeInterval);
  }

  template<size_t D, typename real>
  inline void
    GridSolver<D, real>::endAdvanceTimeStep(double timeInterval)
  {
    // Invoke callback
    onEndAdvanceTimeStep(timeInterval);
  }

  template<size_t D, typename real>
  inline void
    GridSolver<D, real>::updateCollider(double timeInterval)
  {
    if (_collider != nullptr)
      _collider->update(this->currentTime(), timeInterval);
  }

  template<size_t D, typename real>
  inline void
    GridSolver<D, real>::updateEmitter(double timeInterval)
  {
    // TODO
  }

} // end namespace cg

#endif // __GridSolver_h
