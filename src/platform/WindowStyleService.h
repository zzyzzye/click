#pragma once

#include <memory>

class QWidget;

class WindowStyleService {
 public:
  virtual ~WindowStyleService() = default;
  virtual void prepare(QWidget* window) = 0;
  virtual void apply(QWidget* window) = 0;
  virtual bool usesBackdrop() const = 0;
};

std::unique_ptr<WindowStyleService> createWindowStyleService();
