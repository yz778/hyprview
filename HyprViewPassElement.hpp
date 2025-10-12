#pragma once
#include <hyprland/src/render/pass/PassElement.hpp>

class CHyprView;

class CHyprViewPassElement : public IPassElement {
public:
  CHyprViewPassElement(CHyprView *instance_);
  virtual ~CHyprViewPassElement() = default;

  virtual void draw(const CRegion &damage);
  virtual bool needsLiveBlur();
  virtual bool needsPrecomputeBlur();
  virtual std::optional<CBox> boundingBox();
  virtual CRegion opaqueRegion();

  virtual const char *passName() { return "CHyprViewPassElement"; }

private:
  CHyprView *instance;
};