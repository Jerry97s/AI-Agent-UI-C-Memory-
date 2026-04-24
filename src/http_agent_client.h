#pragma once

#include <string>
#include <string_view>

struct AgentHealth
{
  std::string status;
  std::string version;
  std::string mode;
  std::string model;
};

class HttpAgentClient
{
public:
  explicit HttpAgentClient(std::string base_url);

  // Throws std::runtime_error on failure.
  AgentHealth GetHealth() const;

  // Throws std::runtime_error on failure.
  std::string Chat(std::string_view message) const;

private:
  std::string base_url_;
};

