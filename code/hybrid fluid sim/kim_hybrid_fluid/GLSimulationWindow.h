#ifndef __GLSimulationWindow_h
#define __GLSimulationWindow_h

#include "graphics/GLBuffer.h"
#include "graphics/GLGraphicsBase.h"
#include "graphics/GLRenderWindow2.h"
#include "utils/Stopwatch.h"
#include "OldPicSolver.h"
#include "GridSolver.h"
#include "GLDrawSpheres.h"
#include "ParticleEmitterSet.h"
#include "Sphere.h"
#include <math.h>

namespace cg
{

  namespace
  {

    template <typename real>
    inline real
      Trand()
    {
      ASSERT_REAL(real, "Trand: only floating point types allowed");
      return (real)rand() / RAND_MAX;
    }

    template <typename real>
    inline real
      Rrand(real a, real b)
    {
      return a + Trand<real>() * (b - a);
    }

    inline vec2f
      vrand()
    {
      return { Trand<float>(), Trand<float>() };
    }

    inline vec2f
      RVrand(const vec2f& a, const vec2f& b)
    {
      return { Rrand(a.x, b.x), Rrand(a.y, b.y) };
    }

    inline vec2f
      randPoint(const Bounds2f& bounds)
    {
      return RVrand(bounds.min(), bounds.max());
    }

  } // end anonymous namespace

  template <typename real>
  class GLSimulationWindow final : public GLRenderWindow2
  {
  public:
    ASSERT_REAL(real, "GLSimulationWindow: floating point type expected");

    using vec_type = Vector<real, 2>;

    GLSimulationWindow(const char* title, int width, int height) :
      GLRenderWindow2(title, width, height),
      _program{ "GLRenderer" }
    {
      // do nothing
    }

    virtual ~GLSimulationWindow()
    {
      delete _solver;
    }

  protected:
    void initialize() override;
    void renderScene() override;
    void gui() override;

  private:

    struct EmitterConfig
    {
      vec2f min{ 0.0f }, max{ 2.0f };
      vec2f center{ 0.0f };
      float radius{ 0.2f };
      float particleSpacing{ 1.0f / 128.0f };
      bool isBox{ true };
    };

    using Base = GLRenderWindow2;
    float _scale{ 0.0f };

    bool _paused{ true };
    bool _drawGrid{ false };
    bool _enableColorMap{ false };
    double _radius = 0.005;
    vec3f camera{ 0.0f, 0.0f, 1.728524f };
    vec4f _particleColor{ cg::Color::white };
    GLSL::Program _program;
    Reference<GLBuffer<vec_type>> _positions;
    Reference<GLBuffer<vec_type>> _velocities;
    Reference<GLBuffer<real>> _alphas;
    GLuint _vao;
    GLuint _vbo;
    GLuint _ebo;

    Frame _frame;
    GridSolver<2, real>* _solver{ nullptr };
    bool _useAdaptiveTimeStepping{ false };
    size_t _numberOfFixedSubTimeSteps = 5;
    size_t _gridSize{ 100 };
    float _viscosity = 0.0f;
    vec2f _gravity{ 0.0f, -9.8f };
    // emitters
    std::vector<EmitterConfig> _emitterConfigs;
    bool _ready{ false };
    vec2f _bmin{ 0.0f };
    vec2f _bmax{ 1.0f };
    float _particleSpacing{ 1.0f / 128.0f };
    size_t _numberOfParticles{ 10000 };

    void resetSimulation();
    void surfaceOptions();
    void boxEmitterOptions(EmitterConfig&, int);
    void sphereEmitterOptions(EmitterConfig&, int);
    void drawGrid();
    void drawEmitter();
    void buildSolver();
    Index2 mouseToGridIndex();
    Index2 mouseToGridIndex(double, double);

    enum class MoveBits
    {
      Left = 1,
      Right = 2,
      Up = 4,
      Down = 8,
    };

    enum class DragBits
    {
      Source = 1,
      Force = 2
    };

    Reference<GLGraphics2> _g2;
    Flags<MoveBits> _moveFlags{};
    Flags<DragBits> _dragFlags{};
    int _pivotX;
    int _pivotY;
    int _mouseX;
    int _mouseY;

    bool mouseButtonInputEvent(int, int, int) override;
    bool mouseMoveEvent(double, double) override;

    Index2 _source_pos = Index2{-1, -1};
    vec_type _force_dir;


    real func1(vec2f);
    real func2(vec2f);
    real func3(vec2f);
  }; // GLSimulationWindow

  template <typename real>
  inline real
    GLSimulationWindow<real>::func1(vec2f v)
  {
    return math::clamp<real>(abs(tanh(v.x) + cos(v.y)), 0, 1);
  }

  template <typename real>
  inline real
    GLSimulationWindow<real>::func2(vec2f v)
  {
    return Rrand(0.f, 1.f);
  }

  template <typename real>
  inline real
    GLSimulationWindow<real>::func3(vec2f v)
  {
    return math::clamp<real>(v.y, 0, 1);
  }

  template <typename real>
  inline void
    GLSimulationWindow<real>::initialize()
  {
    Base::initialize();
    auto bgColor = Color::black;
    bgColor.a = 1;
    this->backgroundColor = bgColor;
    _program.setShaders(vertexShader, fragmentShader);
    //_program.setShader(GL_GEOMETRY_SHADER, geometryShader);
    _program.use();

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glClearColor(0, 0, 0, 1.f);
    glGenVertexArrays(1, &_vao);
    glGenBuffers(1, &_ebo);
    glBindVertexArray(_vao);

    _solver = new GridSolver<2, real>(
      Index2(_gridSize, _gridSize),
      vec_type(static_cast<real>(0.1f)),
      vec_type(static_cast<real>(-1.0f)));
    _program.setUniform("radius", (float)_radius);
    // TODO: allow window resize
    _program.setUniformVec2("viewportSize", cg::vec2f{ 1270, 720 });

    _positions = new GLBuffer<vec_type>(1);
    _velocities = new GLBuffer<vec_type>(1);
    _alphas = new GLBuffer<real>(1);

    auto glType = GL_FLOAT;
    if (std::is_same<real, double>::value)
      glType = GL_DOUBLE;

    _positions->bind();
    glVertexAttribPointer(0, 2, glType, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    _velocities->bind();
    glVertexAttribPointer(1, 2, glType, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(1);
    _alphas->bind();
    glVertexAttribPointer(2, 1, glType, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(2);

    _emitterConfigs.push_back(EmitterConfig());
  }

  template<typename real>
  inline void
  GLSimulationWindow<real>::buildSolver()
  {
    _ready = true;
    auto origin = vec_type{ static_cast<real>(-1.0f) };
    _solver = new GridSolver<2, real>(
      Index2{ int64_t(_gridSize) },
      vec_type{ static_cast<real>(_emitterConfigs[0].max.x / _gridSize) },
      origin
      );
    auto data = _solver->density();
    auto n = _solver->size();
    auto i = 0;
    for (auto& v : *data)
    {
      v = func3(data->dataOrigin() + vec_type((i / n.x), (i % n.y)) * data->cellSize());
      i++;
      if (i < n.x / 2)
        v = 0.5;
    }
    data->operator[](Index2(1, 3)) = 1.f;
    _solver->setGravity(vec_type{ _gravity });
    _solver->setViscosityCoefficient(_viscosity);

    _solver->advanceFrame(_frame++);

    _solver->setIsUsingFixedSubTimeSteps(!_useAdaptiveTimeStepping);
    _solver->setNumberOfSubTimeSteps(_numberOfFixedSubTimeSteps);

    auto d_size = _solver->density()->size();
    auto v_size = _solver->velocity()->size();

    glBindVertexArray(_vao);

    _positions->bind();
    _positions->resize(d_size.x*d_size.y);
    auto size = data->size();
    std::vector<vec_type> positions(size.x * size.y);
    for (size_t i = 0; i < _positions->size(); i++)
    {
      positions[i] = data->dataPosition(Index2{ Index2::base_type(i / size.x), Index2::base_type(i % size.y) });
    }
    _positions->setData(positions.data());

    _velocities->bind();
    _velocities->resize(v_size.x * v_size.y);
    
    _alphas->bind();
    _alphas->resize(d_size.x * d_size.y);
    std::vector<real> alphas(d_size.x * d_size.y);
    for (size_t i = 0; i < _alphas->size(); i++)
      alphas[i] = data->sample(data->dataOrigin() + vec_type((i / d_size.x), (i % d_size.y)) * data->cellSize());
    _alphas->setData(alphas.data());

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);
    std::vector<int> indices((d_size.x -1) * (d_size.y-1) * 6);
    int index = 0;
    for (int x = 0; x < d_size.x-1; x++)
    {
      for (int z = 0; z < d_size.y - 1; z++)
      {
        int offset = x * d_size.x + z;
        indices[index] = (offset + 0);
        indices[index + 1] = (offset + 1);
        indices[index + 2] = (offset + d_size.x);
        indices[index + 3] = (offset + 1);
        indices[index + 4] = (offset + d_size.x + 1);
        indices[index + 5] = (offset + d_size.x);
        index += 6;
      }
    }
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(int)*indices.size(), indices.data(), GL_STATIC_DRAW);


  }

  template <typename real>
  void
    GLSimulationWindow<real>::gui()
  {
    ImGui::Begin("Simulation Controller");
    ImGui::Checkbox("Draw Solver Grid", &_drawGrid);
    if (_ready)
    {
      //ImGui::Text("Number of Particles: %ull", _solver->size.x*size.yparticleSystem().size());
      if (ImGui::Button("Pause"))
        _paused = !_paused;
      if (ImGui::Button("Advance"))
      {
        _solver->advanceFrame(_frame++);
      }
    }
    else
    {
      if (ImGui::CollapsingHeader("Solver"))
      {
        if (ImGui::Button(
          "Enable AdaptiveTimeStepping\0Enable Fixed SubTimeStepping" + (int(_useAdaptiveTimeStepping) * 28)))
        {
          _useAdaptiveTimeStepping = !_useAdaptiveTimeStepping;
        }
        if (!_useAdaptiveTimeStepping)
        {
          ImGui::DragInt("Number of SubTimeSteps", (int*)&_numberOfFixedSubTimeSteps, 1.0f, 1, 10);
        }
        ImGui::DragInt("Grid Size", (int*)&_gridSize, 1.0f, 1, 200);
        ImGui::InputInt("Max Number of Particles", (int*)&_numberOfParticles, 50, 200);
        ImGui::DragFloat2("Gravity", (float*)&_gravity, 0.2f);
        ImGui::DragFloat("Viscosity", (float*)&_viscosity, 0.01f, 0.0f, 1.0f);
      }
      if (ImGui::CollapsingHeader("Particle Emmiter"))
      {
        surfaceOptions();
        ImGui::ColorEdit3("Particles Color", (float*)&_particleColor);
      }
      if (_emitterConfigs.size() > 0)
        if (ImGui::Button("Build Solver"))
          buildSolver();
    }

    ImGui::End();
  }

  template <typename real>
  void
    GLSimulationWindow<real>::surfaceOptions()
  {
    const char* items[] = { "Box", "Sphere" };
    for (int i = 0; i < _emitterConfigs.size(); ++i)
    {
      auto& config = _emitterConfigs[i];
      const char* current_item = config.isBox ? items[0] : items[1];
      auto label = "##SurfaceType" + std::to_string(i);
      if (ImGui::BeginCombo(label.c_str(), current_item, ImGuiComboFlags_NoArrowButton))
      {
        for (int n = 0; n < IM_ARRAYSIZE(items); ++n)
        {
          bool selected = (current_item == items[n]);
          if (ImGui::Selectable(items[n], selected))
          {
            current_item = items[n];
            if (n == 0)
              config.isBox = true;
            else
              config.isBox = false;
          }
          if (selected)
            ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
      }
      if (config.isBox) // box
        boxEmitterOptions(config, i);
      else
        sphereEmitterOptions(config, i);
    }
  }

  template <typename real>
  inline void
    GLSimulationWindow<real>::boxEmitterOptions(EmitterConfig& config, int id)
  {
    auto l = "##" + std::to_string(id);
    ImGui::DragFloat2(("Min" + l).c_str(), (float*)&config.min, 0.1f, 0.0f, 2.0f);
    ImGui::DragFloat2(("Max" + l).c_str(), (float*)&config.max, 0.1f, 0.05f, 2.0f);
    ImGui::DragFloat(("Particle Spacing" + l).c_str(), (float*)&config.particleSpacing, 0.001f, 1.0f / 200.0f, 0.2f);

    _bmin.x = math::clamp<real>(config.min.x, 0.0f, config.max.x);
    _bmin.y = math::clamp<real>(config.min.y, 0.0f, config.max.y);
  }

  template<typename real>
  inline void
    GLSimulationWindow<real>::sphereEmitterOptions(EmitterConfig& config, int id)
  {
    auto l = "##" + std::to_string(id);
    ImGui::DragFloat2(("Center" + l).c_str(), (float*)&config.center, 0.1f, 0.0f, 2.0f);
    ImGui::DragFloat(("Radius" + l).c_str(), (float*)&config.radius, 0.05f, 0.05f, 0.5f);
    ImGui::DragFloat(("Particle Spacing" + l).c_str(), (float*)&config.particleSpacing, 0.001f, 1.0f / 200.0f, 0.2f);
  }

  template <typename real>
  inline void
    GLSimulationWindow<real>::renderScene()
  {
    clear(this->backgroundColor);

    if (!_ready)
    {
      drawEmitter();
      return;
    }

    if (!_paused)
      _solver->advanceFrame(_frame++);

    _program.use();
    glBindVertexArray(_vao);

    const auto& data = _solver->density();
    
    /*_alphas->bind();
    std::vector<real> alphas(size.x * size.y);
    for (size_t i = 0; i < _alphas->size(); i++)
      alphas[i] = data->sample(data->dataOrigin() + vec_type((i / size.x), (i % size.y)) * data->cellSize());
    _alphas->setData(alphas.data());*/
    
    auto projectionMatrix = cg::mat4f::perspective(
      60,
      ((float)width()) / height(),
      0.001f,
      100
    );

    auto mvMatrix = cg::mat4f::TRS(
      camera,
      cg::vec3f::null(),
      cg::vec3f{ 1.0f }
    );
    mvMatrix.invert();

    _program.setUniform("use_color_map", _enableColorMap);
    _program.setUniformMat4("projectionMatrix", projectionMatrix);
    _program.setUniformMat4("mvMatrix", mvMatrix);
    _program.setUniformVec4("color", _particleColor);

    auto size = data->size();
    //glDrawArrays(GL_POINTS, 0, size.x * size.y);
    glDrawElements(GL_TRIANGLES, 6* size.x * size.y, GL_UNSIGNED_INT, 0);
    if (_source_pos.x != -1 && _source_pos.y != -1)
      printf("%d, %d\n", _source_pos.x, _source_pos.y);
    if (_drawGrid)
      drawGrid();
  }


  template <typename real>
  void
    GLSimulationWindow<real>::resetSimulation()
  {
  }

  template <typename real>
  void
    GLSimulationWindow<real>::drawGrid()
  {
    auto mvMatrix = cg::mat4f::TRS(
      camera,
      cg::vec3f::null(),
      cg::vec3f{ 1.0f }
    );
    mvMatrix.invert();

    auto m = mvMatrix;

    auto graphics = this->g2();
    const auto& grid = _solver->velocity();
    auto bounds = Bounds2f{ vec2f{-1.0f}, vec2f{1.0f} };
    auto gridSize = Index2{ Index2::base_type(_gridSize) };
    cg::vec2f gridSpacing{ 2.0f / _gridSize };
    cg::vec2f start{ bounds.min() };
    start = m.transform(cg::vec4f{ start.x, start.y, 0.0f, 1.0f });
    cg::vec2f end{ bounds.max().x, bounds.min().y };
    end = m.transform(cg::vec4f{ end.x, end.y, 0.0f, 1.0f });
    auto color = cg::Color::red;
    color.a = 1;
    graphics->setLineColor(color);

    for (int i = 0; i < gridSize.y + 1; ++i)
    {
      graphics->drawLine(start, end);
      start.y += gridSpacing.y;
      end.y = start.y;
    }
    start = m.transform(cg::vec4f{ bounds.min().x, bounds.min().y, 0.0f, 1.0f });
    end = { bounds.min().x, bounds.max().y };
    end = m.transform(cg::vec4f{ end.x, end.y, 0.0f, 1.0f });
    for (int i = 0; i < gridSize.x + 1; ++i)
    {
      graphics->drawLine(start, end);
      start.x += gridSpacing.x;
      end.x = start.x;
    }
  }

  template<typename real>
  inline void cg::GLSimulationWindow<real>::drawEmitter()
  {
    auto origin = vec2f{ -1.0f };
    auto graphics = this->g2();
    auto polygonMode = graphics->polygonMode();
    graphics->setPolygonMode(GLGraphicsBase::LINE);
    auto color = Color::magenta;
    color.a = 1.f;
    graphics->setLineColor(color);

    for (const auto& conf : _emitterConfigs)
    {
      if (conf.isBox) // box
        graphics->drawBounds(Bounds2f{ conf.min + origin, conf.max + origin });
      else // sphere
        graphics->drawCircumference(conf.center + origin, conf.radius);
    }

    graphics->setPolygonMode(polygonMode);
  }

  template<typename real>
  inline Index2 cg::GLSimulationWindow<real>::mouseToGridIndex()
  {
    int i, j;
    int xPos, yPos;

    cursorPosition(xPos, yPos);
    float w = width() - 1;
    float h = height();
    i = _solver->size().x * xPos / w;
    j = _solver->size().y * (h - yPos) / h;

    return Index2(i, j);
  }

  template<typename real>
  inline Index2 cg::GLSimulationWindow<real>::mouseToGridIndex(double xPos, double yPos)
  {
    float w = width() - 1;
    float h = height();

    auto size = _solver->size();
    int i = size.x * xPos / w ;
    int j = size.y * (h - yPos) / h;

    if (i < 0 || i > size.x-1)
      i = -1;
    if (j < 0 || j > size.y-1)
      j = -1;

    return Index2(i, j);
  }

  template<typename real>
  inline bool
  GLSimulationWindow<real>::mouseButtonInputEvent(int button, int actions, int mods)
  {
    if (ImGui::GetIO().WantCaptureMouse)
      return false;
    (void)mods;

    auto active = actions == GLFW_PRESS;

    if (button == GLFW_MOUSE_BUTTON_RIGHT)
      _dragFlags.enable(DragBits::Force, active);
    else if (button == GLFW_MOUSE_BUTTON_LEFT && actions == GLFW_RELEASE)
    {
      _dragFlags.enable(DragBits::Source, false);
      _source_pos = Index2(-1, -1);
    }
    else if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
      _dragFlags.enable(DragBits::Source, active);
      _source_pos = mouseToGridIndex();
    }
    if (_dragFlags)
      cursorPosition(_pivotX, _pivotY);
    return true;
  }

  template<typename real>
  inline bool
  GLSimulationWindow<real>::mouseMoveEvent(double xPos, double yPos)
  {
    if (!_dragFlags || _solver==NULL)
      return false;
    _mouseX = (int)xPos;
    _mouseY = (int)yPos;

    const auto dx = (_pivotX - _mouseX);
    const auto dy = (_pivotY - _mouseY);

    _pivotX = _mouseX;
    _pivotY = _mouseY;
    if (dx != 0 || dy != 0)
    {
      if (_dragFlags.isSet(DragBits::Source))
      {
        // TODO: pan
        _source_pos = mouseToGridIndex(_mouseX, _mouseY);
      }
      if (_dragFlags.isSet(DragBits::Force))
      {
        _force_dir = vec_type(dx, dy);
      }
    }
    return true;
  }
} // end namespace cg

#endif // __GLSimulationWindow_h