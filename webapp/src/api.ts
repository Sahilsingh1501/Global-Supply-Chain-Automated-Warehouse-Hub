export type User = { id: string; name: string; email: string; role: string };

const TOKEN_KEY = "northstar.session";

export function getToken() {
  return localStorage.getItem(TOKEN_KEY);
}

export function clearSession() {
  localStorage.removeItem(TOKEN_KEY);
}

export async function request<T>(path: string, options: RequestInit = {}): Promise<T> {
  const token = getToken();
  let response: Response;
  try {
    response = await fetch(`/api${path}`, {
      ...options,
      headers: {
        ...(options.body ? { "Content-Type": "application/json" } : {}),
        ...(token ? { Authorization: `Bearer ${token}` } : {}),
        ...options.headers,
      },
    });
  } catch (error) {
    if (error instanceof TypeError) {
      throw new Error(
        "Could not reach the local API. Keep the launcher windows open and check the API window. If Windows Security blocked gsc-server.exe, review Protection history.",
      );
    }
    throw error;
  }
  if (response.status === 204) return undefined as T;
  const payload = await response.json().catch(() => ({})) as { error?: string };
  if (!response.ok) {
    if (response.status === 401) clearSession();
    throw new Error(payload.error || `Request failed (${response.status}).`);
  }
  return payload as T;
}

export async function signIn(email: string, password: string) {
  const result = await request<{ token: string; user: User }>("/auth/login", {
    method: "POST",
    body: JSON.stringify({ email, password }),
  });
  localStorage.setItem(TOKEN_KEY, result.token);
  return result.user;
}
