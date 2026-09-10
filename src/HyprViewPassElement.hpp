#pragma once
#include <hyprland/src/render/pass/PassElement.hpp>

class CHyprView;

class CHyprViewPassElement : public IPassElement {
public:
  CHyprViewPassElement(CHyprView *instance_);
  virtual ~CHyprViewPassElement() = default;

  virtual std::vector<UP<IPassElement>> draw();
  virtual bool needsLiveBlur();
  virtual bool needsPrecomputeBlur();
  virtual std::optional<CBox> boundingBox();
  virtual CRegion opaqueRegion();

  virtual const char *passName() { return "CHyprViewPassElement"; }

  virtual ePassElementType type() { return EK_CUSTOM; }

private:
  CHyprView *instance;
};