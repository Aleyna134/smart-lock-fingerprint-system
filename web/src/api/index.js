const BASE_URL = ""

async function request(method, path, body) {
  const res = await fetch(`${BASE_URL}${path}`, {
    method,
    headers: { "Content-Type": "application/json" },
    body: body ? JSON.stringify(body) : undefined,
  })
  const data = await res.json()
  if (!res.ok) throw new Error(data.error || "An error occurred")
  return data
}

export const api = {
  login: (email, password) => request("POST", "/api/login", { email, password }),
  getStatus: () => request("GET", "/api/status"),
  getLogs: () => request("GET", "/api/logs"),
  getUsers: () => request("GET", "/api/users"),
  addUser: (name, email, password, role, admin_id) =>
    request("POST", "/api/users", { name, email, password, role, admin_id }),
  deleteUser: (id) => request("DELETE", `/api/users/${id}`),
}
