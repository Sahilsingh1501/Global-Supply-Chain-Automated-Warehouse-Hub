import { useCallback, useEffect, useMemo, useState } from "react";
import {
  Activity, ArrowDownLeft, ArrowDownRight, ArrowLeftRight, ArrowRight, Bell, Boxes,
  Building2, Check, ChevronDown, Clock3, FileBarChart2, LayoutDashboard,
  Download, LockKeyhole, LogOut, Mail, MapPin, Menu, Package, Plus, Route, Search, Settings2, ShieldCheck, Truck, Users, X,
} from "lucide-react";
import {
  Area, AreaChart, CartesianGrid, ResponsiveContainer, Tooltip, XAxis, YAxis,
} from "recharts";
import { clearSession, getToken, request, signIn, type User } from "./api";

type Section = "overview" | "inventory" | "warehouses" | "suppliers" | "orders" | "routes" | "shipments" | "reports" | "team" | "activity";
type Row = Record<string, unknown>;
type Dashboard = {
  metrics: { products: number; warehouses: number; totalUnits: number; inTransit: number; delayed: number; openOrders: number; lowStock: number };
  movementByDay: { day: string; received: number; dispatched: number }[];
  lowStock: Row[];
  recentShipments: Row[];
  recentMovements: Row[];
};
type Field = {
  name: string;
  label: string;
  type?: string;
  required?: boolean;
  options?: { value: string; label: string }[];
  optionsFor?: (values: Record<string, string>) => { value: string; label: string }[];
  emptyLabel?: (values: Record<string, string>) => string;
  clearOnChange?: string[];
  wide?: boolean;
  min?: number;
};
type ModalState = { title: string; description: string; fields: Field[]; values?: Record<string, string>; submit: (data: Record<string, string>) => Promise<void> };
type Notice = { type: "success" | "error"; message: string };

const sections: { id: Section; label: string; icon: typeof LayoutDashboard; group: string; roles?: string[] }[] = [
  { id: "overview", label: "Overview", icon: LayoutDashboard, group: "Workspace" },
  { id: "inventory", label: "Inventory", icon: Package, group: "Workspace" },
  { id: "warehouses", label: "Warehouses", icon: Building2, group: "Workspace" },
  { id: "suppliers", label: "Suppliers", icon: Users, group: "Workspace" },
  { id: "orders", label: "Purchase orders", icon: Boxes, group: "Operations" },
  { id: "routes", label: "Route network", icon: Route, group: "Operations" },
  { id: "shipments", label: "Shipments", icon: Truck, group: "Operations" },
  { id: "reports", label: "Reports", icon: FileBarChart2, group: "Insights" },
  { id: "activity", label: "Audit activity", icon: Activity, group: "Insights", roles: ["ADMIN"] },
  { id: "team", label: "Team access", icon: ShieldCheck, group: "Administration", roles: ["ADMIN"] },
];

const toRows = (value: unknown): Row[] => Array.isArray(value) ? value as Row[] : [];
const text = (value: unknown, fallback = "—") => value === null || value === undefined || value === "" ? fallback : String(value);
const number = (value: unknown) => Number(value || 0);
const formatDate = (value: unknown) => value ? new Date(String(value)).toLocaleDateString("en-US", { month: "short", day: "numeric", year: "numeric" }) : "—";
const formatTime = (value: unknown) => value ? new Date(String(value)).toLocaleString("en-US", { month: "short", day: "numeric", hour: "numeric", minute: "2-digit" }) : "—";
const initials = (name: string) => name.split(/\s+/).map((part) => part[0]).slice(0, 2).join("").toUpperCase();

export default function App() {
  const [user, setUser] = useState<User | null>(null);
  const [checking, setChecking] = useState(Boolean(getToken()));
  const [section, setSection] = useState<Section>("overview");
  const [dashboard, setDashboard] = useState<Dashboard | null>(null);
  const [rows, setRows] = useState<Row[]>([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState("");
  const [notice, setNotice] = useState<Notice | null>(null);
  const [modal, setModal] = useState<ModalState | null>(null);
  const [tracking, setTracking] = useState<Row | null>(null);
  const [sidebarOpen, setSidebarOpen] = useState(false);
  const [search, setSearch] = useState("");

  const visibleSections = useMemo(() => sections.filter((item) => !item.roles || item.roles.includes(user?.role || "")), [user]);

  useEffect(() => {
    if (!getToken()) return;
    request<User>("/auth/me").then(setUser).catch(() => { clearSession(); setUser(null); }).finally(() => setChecking(false));
  }, []);

  const loadData = useCallback(async () => {
    if (!user) return;
    setLoading(true);
    setError("");
    try {
      if (section === "overview") {
        setDashboard(await request<Dashboard>("/dashboard"));
      } else {
        const endpoint: Record<Exclude<Section, "overview">, string> = {
          inventory: `/products${search ? `?search=${encodeURIComponent(search)}` : ""}`,
          warehouses: "/warehouses", suppliers: "/suppliers", orders: "/purchase-orders",
          routes: "/routes", shipments: "/shipments", reports: "/reports/stock",
          team: "/users", activity: "/audit",
        };
        setRows(toRows(await request<unknown>(endpoint[section])));
      }
    } catch (reason) {
      setError(reason instanceof Error ? reason.message : "Could not load this view.");
    } finally {
      setLoading(false);
    }
  }, [section, user, search]);

  useEffect(() => { void loadData(); }, [loadData]);
  useEffect(() => {
    if (!notice) return;
    const timer = window.setTimeout(() => setNotice(null), 4000);
    return () => window.clearTimeout(timer);
  }, [notice]);

  useEffect(() => {
    function handleShortcut(event: KeyboardEvent) {
      if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === "k") {
        event.preventDefault();
        document.querySelector<HTMLInputElement>(".global-search input")?.focus();
      } else if (event.key === "/" && !(event.target instanceof HTMLInputElement) && !(event.target instanceof HTMLTextAreaElement)) {
        event.preventDefault();
        document.querySelector<HTMLInputElement>(".table-search input")?.focus();
      }
    }
    window.addEventListener("keydown", handleShortcut);
    return () => window.removeEventListener("keydown", handleShortcut);
  }, []);

  async function handleSignIn(email: string, password: string) {
    const signedIn = await signIn(email, password);
    setUser(signedIn);
    setNotice({ type: "success", message: `Welcome back, ${signedIn.name.split(" ")[0]}.` });
  }

  function handleSignOut() {
    clearSession();
    setUser(null);
    setDashboard(null);
    setRows([]);
  }

  function openModal(value: ModalState) {
    setModal(value);
  }

  async function submitModal(data: Record<string, string>) {
    if (!modal) return;
    await modal.submit(data);
    setModal(null);
    setNotice({ type: "success", message: "Changes saved successfully." });
    await loadData();
  }

  function formModal(title: string, description: string, fields: Field[], submit: ModalState["submit"], values?: Record<string, string>) {
    openModal({ title, description, fields, submit, values });
  }

  async function createForSection(target: Section) {
    const products = toRows(await request<unknown>("/products"));
    const warehouses = toRows(await request<unknown>("/warehouses"));
    const suppliers = toRows(await request<unknown>("/suppliers"));
    const routes = target === "shipments" ? toRows(await request<unknown>("/routes")) : [];
    const productOptions = products.map((product) => ({ value: text(product.id), label: `${text(product.name)} · ${text(product.sku)}` }));
    const warehouseOptions = warehouses.map((warehouse) => ({ value: text(warehouse.id), label: `${text(warehouse.name)} · ${text(warehouse.city)}` }));
      if (target === "inventory") {
      formModal("Add a product", "Create a catalog item for your operations.", [
        { name: "sku", label: "SKU", required: true }, { name: "name", label: "Product name", required: true },
        { name: "category", label: "Category", required: true }, { name: "unit", label: "Unit", required: true },
        { name: "reorderLevel", label: "Reorder at", type: "number", required: true }, { name: "description", label: "Description", wide: true },
      ], (data) => request("/products", { method: "POST", body: JSON.stringify({ ...data, reorderLevel: Number(data.reorderLevel) }) }));
    } else if (target === "warehouses") {
      formModal("Add a warehouse", "Create a new fulfillment location.", [
        { name: "code", label: "Warehouse code", required: true }, { name: "name", label: "Name", required: true },
        { name: "city", label: "City", required: true }, { name: "capacity", label: "Capacity (units)", type: "number", required: true },
        { name: "address", label: "Street address", wide: true },
      ], (data) => request("/warehouses", { method: "POST", body: JSON.stringify({ ...data, capacity: Number(data.capacity) }) }));
    } else if (target === "suppliers") {
      formModal("Add a supplier", "Keep supplier contact details in one place.", [
        { name: "code", label: "Supplier code", required: true }, { name: "name", label: "Company name", required: true },
        { name: "email", label: "Email", type: "email" }, { name: "phone", label: "Phone" }, { name: "address", label: "Location", wide: true },
      ], (data) => request("/suppliers", { method: "POST", body: JSON.stringify(data) }));
    } else if (target === "orders") {
      const supplierOptions = suppliers.map((supplier) => ({ value: text(supplier.id), label: text(supplier.name) }));
      formModal("Create a purchase order", "Choose a supplier, product, quantity, and receiving warehouse. Stock increases when you record the delivery.", [
        { name: "supplierId", label: "Supplier", type: "select", required: true, options: supplierOptions },
        { name: "warehouseId", label: "Deliver to", type: "select", required: true, options: warehouseOptions },
        { name: "productId", label: "Product", type: "select", required: true, options: productOptions },
        { name: "quantityOrdered", label: "Quantity", type: "number", required: true },
        { name: "expectedAt", label: "Expected delivery", type: "date" }, { name: "notes", label: "Order notes", wide: true },
      ], (data) => request("/purchase-orders", { method: "POST", body: JSON.stringify({ supplierId: data.supplierId, warehouseId: data.warehouseId, expectedAt: data.expectedAt ? new Date(`${data.expectedAt}T12:00:00`).toISOString() : null, notes: data.notes, items: [{ productId: data.productId, quantityOrdered: Number(data.quantityOrdered) }] }) }));
    } else if (target === "routes") {
      formModal("Add a shipping lane", "Connect two warehouses to enable shipments.", [
        { name: "fromWarehouseId", label: "Origin warehouse", type: "select", required: true, options: warehouseOptions },
        { name: "toWarehouseId", label: "Destination warehouse", type: "select", required: true, options: warehouseOptions },
        { name: "distanceKm", label: "Distance (km)", type: "number", required: true, min: 1 },
        { name: "transitHours", label: "Transit time (hours)", type: "number", required: true },
      ], (data) => request("/routes", { method: "POST", body: JSON.stringify({ ...data, distanceKm: Number(data.distanceKm), transitHours: Number(data.transitHours) }) }));
    } else if (target === "shipments") {
      formModal("Create a shipment", "Dispatch available stock along an active route.", [
        { name: "originId", label: "Origin warehouse", type: "select", required: true, options: warehouseOptions, clearOnChange: ["destinationId", "productId", "quantity"] },
        {
          name: "destinationId", label: "Destination warehouse", type: "select", required: true,
          optionsFor: (values) => {
            const originId = values.originId;
            if (!originId) return [];
            const connectedIds = new Set(routes.filter((route) =>
              route.active === true &&
              (text(route.fromWarehouseId) === originId || text(route.toWarehouseId) === originId),
            ).map((route) => text(route.fromWarehouseId) === originId
              ? text(route.toWarehouseId)
              : text(route.fromWarehouseId)));
            return warehouses.filter((warehouse) => connectedIds.has(text(warehouse.id)))
              .map((warehouse) => ({ value: text(warehouse.id), label: `${text(warehouse.name)} · ${text(warehouse.city)}` }));
          },
          emptyLabel: (values) => values.originId ? "No active routes from this warehouse" : "Select an origin first",
        },
        {
          name: "productId", label: "Product", type: "select", required: true,
          optionsFor: (values) => {
            const warehouse = warehouses.find((item) => text(item.id) === values.originId);
            return toRows(warehouse?.stock)
              .filter((stock) => number(stock.quantity) > 0)
              .map((stock) => {
                const productId = text(stock.productId);
                const product = products.find((item) => text(item.id) === productId);
                return {
                  value: productId,
                  label: `${text(product?.name, text((stock.product as Row | undefined)?.name))} · ${text(product?.sku, text((stock.product as Row | undefined)?.sku))} · ${number(stock.quantity)} available`,
                };
              });
          },
          emptyLabel: (values) => values.originId ? "No products in this warehouse" : "Select an origin first",
          clearOnChange: ["quantity"],
        },
        { name: "quantity", label: "Quantity", type: "number", required: true },
        { name: "expectedAt", label: "Estimated delivery", type: "date" },
      ], (data) => request("/shipments", { method: "POST", body: JSON.stringify({ originId: data.originId, destinationId: data.destinationId, expectedAt: data.expectedAt ? new Date(`${data.expectedAt}T12:00:00`).toISOString() : null, items: [{ productId: data.productId, quantity: Number(data.quantity) }] }) }));
    } else if (target === "team") {
      formModal("Invite a team member", "Create a sign-in for someone in your organization.", [
        { name: "name", label: "Full name", required: true },
        { name: "email", label: "Work email", type: "email", required: true },
        { name: "password", label: "Temporary password", type: "password", required: true },
        { name: "role", label: "Role", type: "select", required: true, options: ["INVENTORY", "LOGISTICS", "PROCUREMENT", "VIEWER", "ADMIN"].map((role) => ({ value: role, label: role.toLowerCase() })) },
      ], (data) => request("/users", { method: "POST", body: JSON.stringify(data) }));
    }
  }

  if (checking) return <div className="boot-screen"><img className="boot-logo" src="/stocksathi-logo.png" alt="StockSathi" /><span className="boot-loader" /></div>;
  if (!user) return <Login onSignIn={handleSignIn} />;

  const active = sections.find((item) => item.id === section) || sections[0];
  const allowedToWrite = (
    (["inventory", "warehouses"].includes(section) && ["ADMIN", "INVENTORY"].includes(user.role)) ||
    (section === "suppliers" && ["ADMIN", "PROCUREMENT"].includes(user.role)) ||
    (section === "orders" && ["ADMIN", "INVENTORY", "PROCUREMENT"].includes(user.role)) ||
    (["routes", "shipments"].includes(section) && ["ADMIN", "LOGISTICS"].includes(user.role)) ||
    (section === "team" && user.role === "ADMIN")
  );
  const allowedToCreate = section === "orders"
    ? ["ADMIN", "PROCUREMENT"].includes(user.role)
    : allowedToWrite;

  return (
    <div className="app-shell">
      {sidebarOpen && <button className="mobile-shade" aria-label="Close navigation" onClick={() => setSidebarOpen(false)} />}
      <aside className={`sidebar ${sidebarOpen ? "sidebar-open" : ""}`}>
        <div className="brand"><img className="brand-logo" src="/stocksathi-logo.png" alt="StockSathi" /><button className="mobile-close icon-button" onClick={() => setSidebarOpen(false)} aria-label="Close menu"><X size={19} /></button></div>
        <div className="workspace-switch"><span className="workspace-avatar">S</span><span className="workspace-copy"><strong>StockSathi Operations</strong><small>Operations workspace</small></span><ChevronDown size={15} /></div>
        <nav className="side-nav">
          {["Workspace", "Operations", "Insights", "Administration"].map((group) => {
            const items = visibleSections.filter((item) => item.group === group);
            return items.length ? <div className="nav-group" key={group}><span className="nav-label">{group}</span>{items.map((item) => {
              const Icon = item.icon;
              return <button key={item.id} className={`nav-item ${section === item.id ? "nav-active" : ""}`} onClick={() => { setSection(item.id); setSidebarOpen(false); setSearch(""); }}><Icon size={17} strokeWidth={1.8} /><span>{item.label}</span>{item.id === "shipments" && (dashboard?.metrics.inTransit || 0) > 0 && <span className="nav-count">{dashboard?.metrics.inTransit}</span>}</button>;
            })}</div> : null;
          })}
        </nav>
        <div className="sidebar-bottom">
          <div className="support-card"><div className="support-icon"><Package size={17} /></div><strong>Your stock, at a glance</strong><p>See products and on-hand quantities across your warehouses.</p><button onClick={() => { setSection("inventory"); setSearch(""); }}>Explore inventory <ArrowRight size={14} /></button></div>
          <div className="profile-card"><div className="avatar">{initials(user.name)}</div><span className="profile-copy"><strong>{user.name}</strong><small>{user.role.toLowerCase().replace("_", " ")}</small></span><button className="icon-button profile-menu" onClick={handleSignOut} title="Sign out" aria-label="Sign out"><LogOut size={17} /></button></div>
        </div>
      </aside>

      <main className="main-area">
        <header className="topbar">
          <button className="icon-button mobile-menu" aria-label="Open navigation" onClick={() => setSidebarOpen(true)}><Menu size={20} /></button>
          <div className="breadcrumb"><span>Workspace</span><span className="crumb-slash">/</span><strong>{active.label}</strong></div>
          <div className="topbar-actions"><label className="global-search"><Search size={16} /><input placeholder={section === "overview" ? "Search inventory..." : `Search ${active.label.toLowerCase()}...`} value={search} onChange={(event) => { if (section === "overview") setSection("inventory"); setSearch(event.target.value); }} /><kbd>Ctrl K</kbd></label><button className="icon-button notification-button" aria-label="Notifications" onClick={() => setSection("overview")}><Bell size={18} />{(dashboard?.metrics.delayed || 0) > 0 && <i />}</button><span className="topbar-divider" /><button className="avatar top-avatar" title={user.email}>{initials(user.name)}</button></div>
        </header>
        <div className="content-area">
          {error && <div className="error-banner"><span>{error}</span><button className="icon-button" aria-label="Dismiss error" onClick={() => setError("")}><X size={16} /></button></div>}
          {section === "overview" && <DashboardView data={dashboard} loading={loading} onNavigate={setSection} />}
          {section !== "overview" && <ModuleView section={section} rows={rows} loading={loading} search={search} canWrite={allowedToWrite} canCreate={allowedToCreate} userRole={user.role} onCreate={() => void createForSection(section)} onAction={(row, action) => {
            if (section === "inventory" && (action === "adjust" || action === "adjust-all")) {
              void request<unknown>("/warehouses").then((value) => {
                const options = toRows(value).map((warehouse) => ({ value: text(warehouse.id), label: text(warehouse.name) }));
                const productFields = action === "adjust-all" ? [{ name: "productId", label: "Product", type: "select", required: true, options: toRows(rows).map((product) => ({ value: text(product.id), label: `${text(product.name)} · ${text(product.sku)}` })) } satisfies Field] : [];
                formModal("Adjust on-hand stock", action === "adjust" ? `Add or remove ${text(row.name)} at a warehouse.` : "Record a stock receipt or correction at a warehouse.", [
                  ...productFields,
                  { name: "warehouseId", label: "Warehouse", type: "select", required: true, options },
                  { name: "delta", label: "Change in units", type: "number", required: true, min: -1_000_000 },
                  { name: "note", label: "Reason", wide: true },
                ], (data) => request("/inventory/adjustments", { method: "POST", body: JSON.stringify({ productId: action === "adjust" ? row.id : data.productId, warehouseId: data.warehouseId, delta: Number(data.delta), note: data.note }) }));
              }).catch((reason) => setNotice({ type: "error", message: reason instanceof Error ? reason.message : "Could not open stock adjustment." }));
            } else if (section === "inventory" && action === "edit") {
              formModal(`Edit ${text(row.name)}`, `Update catalog details for ${text(row.sku)}.`, [
                { name: "name", label: "Product name", required: true },
                { name: "category", label: "Category", required: true },
                { name: "unit", label: "Unit", required: true },
                { name: "reorderLevel", label: "Reorder at", type: "number", required: true, min: 0 },
                { name: "description", label: "Description", wide: true },
              ], (data) => request(`/products/${text(row.id)}`, { method: "PATCH", body: JSON.stringify({ ...data, reorderLevel: Number(data.reorderLevel) }) }), {
                name: text(row.name), category: text(row.category), unit: text(row.unit), reorderLevel: text(row.reorderLevel), description: text(row.description, ""),
              });
            } else if (section === "warehouses" && action === "edit") {
              formModal(`Edit ${text(row.name)}`, `Update details for warehouse ${text(row.code)}.`, [
                { name: "name", label: "Name", required: true },
                { name: "city", label: "City", required: true },
                { name: "capacity", label: "Capacity (units)", type: "number", required: true },
                { name: "address", label: "Street address", wide: true },
              ], (data) => request(`/warehouses/${text(row.id)}`, { method: "PATCH", body: JSON.stringify({ ...data, capacity: Number(data.capacity) }) }), {
                name: text(row.name), city: text(row.city), capacity: text(row.capacity), address: text(row.address, ""),
              });
            } else if (section === "suppliers" && action === "edit") {
              formModal(`Edit ${text(row.name)}`, `Update contact information for ${text(row.code)}.`, [
                { name: "name", label: "Company name", required: true },
                { name: "email", label: "Email", type: "email" },
                { name: "phone", label: "Phone" },
                { name: "address", label: "Location", wide: true },
              ], (data) => request(`/suppliers/${text(row.id)}`, { method: "PATCH", body: JSON.stringify(data) }), {
                name: text(row.name), email: text(row.email, ""), phone: text(row.phone, ""), address: text(row.address, ""),
              });
            } else if (section === "routes" && action === "edit") {
              formModal("Manage shipping lane", `${text((row.fromWarehouse as Row | undefined)?.name)} to ${text((row.toWarehouse as Row | undefined)?.name)}.`, [
                { name: "distanceKm", label: "Distance (km)", type: "number", required: true, min: 1 },
                { name: "transitHours", label: "Transit time (hours)", type: "number", required: true },
                { name: "active", label: "Route status", type: "select", required: true, options: [{ value: "true", label: "Active" }, { value: "false", label: "Paused" }] },
              ], (data) => request(`/routes/${text(row.id)}`, { method: "PATCH", body: JSON.stringify({ distanceKm: Number(data.distanceKm), transitHours: Number(data.transitHours), active: data.active === "true" }) }), {
                distanceKm: text(row.distanceKm), transitHours: text(row.transitHours), active: row.active ? "true" : "false",
              });
            } else if (section === "orders" && action === "receive") {
              const items = toRows(row.items);
              const pending = items.filter((item) => number(item.quantityOrdered) > number(item.quantityReceived));
              if (!pending.length) return;
              formModal("Receive purchase order", `${text(row.orderNumber)} · choose a quantity to receive.`, pending.map((item) => ({ name: text(item.id), label: `${text((item.product as Row | undefined)?.name)} · ${number(item.quantityOrdered) - number(item.quantityReceived)} remaining`, type: "number", required: true })), (data) => request(`/purchase-orders/${text(row.id)}/receive`, { method: "POST", body: JSON.stringify({ items: pending.filter((item) => Number(data[text(item.id)]) > 0).map((item) => ({ itemId: item.id, quantity: Number(data[text(item.id)]) })) }) }));
            } else if (section === "orders" && action === "cancel") {
              if (window.confirm(`Cancel ${text(row.orderNumber)}? Any quantities already received will remain in stock.`)) {
                void request(`/purchase-orders/${text(row.id)}/cancel`, { method: "POST", body: JSON.stringify({}) }).then(async () => { setNotice({ type: "success", message: "Purchase order cancelled." }); await loadData(); }).catch((reason) => setNotice({ type: "error", message: reason instanceof Error ? reason.message : "Could not cancel this purchase order." }));
              }
            } else if (section === "team" && action === "edit") {
              formModal(`Manage ${text(row.name)}`, "Update the member's name, role, or access status.", [
                { name: "name", label: "Full name", required: true },
                { name: "role", label: "Role", type: "select", required: true, options: ["INVENTORY", "LOGISTICS", "PROCUREMENT", "VIEWER", "ADMIN"].map((role) => ({ value: role, label: role.toLowerCase() })) },
                { name: "active", label: "Access", type: "select", required: true, options: [{ value: "true", label: "Active" }, { value: "false", label: "Disabled" }] },
              ], (data) => request(`/users/${text(row.id)}`, { method: "PATCH", body: JSON.stringify({ name: data.name, role: data.role, active: data.active === "true" }) }), {
                name: text(row.name), role: text(row.role), active: row.active ? "true" : "false",
              });
            } else if (section === "shipments" && action === "advance") {
              void request(`/shipments/${text(row.id)}/advance`, { method: "PATCH", body: JSON.stringify({}) }).then(async () => { setNotice({ type: "success", message: "Shipment progress updated." }); await loadData(); }).catch((reason) => setNotice({ type: "error", message: reason instanceof Error ? reason.message : "Could not update shipment." }));
            } else if (section === "shipments" && action === "delay") {
              formModal("Flag a shipment delay", `Add a note for ${text(row.trackingNumber)}.`, [{ name: "reason", label: "Delay reason", required: true, wide: true }], (data) => request(`/shipments/${text(row.id)}/delay`, { method: "PATCH", body: JSON.stringify(data) }));
            } else if (section === "shipments" && action === "track") {
              setTracking(row);
            }
          }} />}
        </div>
        <footer className="app-footer"><span><i className="online-dot" />All systems operational</span><span>StockSathi <span className="footer-divider">·</span> v1.0.0</span></footer>
      </main>

      {modal && <Modal modal={modal} onClose={() => setModal(null)} onSubmit={submitModal} />}
      {tracking && <TrackingModal shipment={tracking} onClose={() => setTracking(null)} />}
      {notice && <div className={`toast ${notice.type}`} role="status"><span className="toast-icon">{notice.type === "success" ? <Check size={16} /> : <X size={16} />}</span>{notice.message}<button className="icon-button" onClick={() => setNotice(null)} aria-label="Dismiss"><X size={16} /></button></div>}
    </div>
  );
}

function Login({ onSignIn }: { onSignIn: (email: string, password: string) => Promise<void> }) {
  const [email, setEmail] = useState("admin@example.com");
  const [password, setPassword] = useState("");
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState("");
  async function submit(event: React.FormEvent) {
    event.preventDefault();
    setBusy(true);
    setError("");
    try { await onSignIn(email, password); } catch (reason) { setError(reason instanceof Error ? reason.message : "Could not sign in."); }
    finally { setBusy(false); }
  }
  return <div className="login-shell">
    <div className="login-art">
      <div className="login-art-content">
        <div className="brand login-brand"><img className="brand-logo" src="/stocksathi-logo.png" alt="StockSathi" /></div>
        <div className="art-message">
          <span className="eyebrow"><i className="online-dot" /> YOUR OPERATIONS, IN FOCUS</span>
          <h1>Move goods.<br /><em>Move forward.</em></h1>
          <p>A smarter, simpler way to keep your supply chain moving. Track inventory, connect warehouses, and deliver with confidence.</p>
          <div className="art-stats">
            <div><Boxes size={22} /><strong>One view</strong><span>for every warehouse</span></div>
            <div><Route size={22} /><strong>Real-time</strong><span>shipment updates</span></div>
          </div>
        </div>
        <div className="art-bottom"><span>Built for the people who keep things moving.</span><span>SUPPLY CHAIN HUB</span></div>
      </div>
      <svg className="login-hero-graphic" viewBox="0 0 760 540" fill="none" aria-hidden="true">
        <defs>
          <linearGradient id="hero-line" x1="106" y1="458" x2="708" y2="67" gradientUnits="userSpaceOnUse"><stop stopColor="#0ACB8B" stopOpacity=".15" /><stop offset=".55" stopColor="#32F1B1" /><stop offset="1" stopColor="#A2FFE2" /></linearGradient>
          <linearGradient id="hero-ocean" x1="398" y1="185" x2="587" y2="438" gradientUnits="userSpaceOnUse"><stop stopColor="#075E53" /><stop offset=".55" stopColor="#063D3D" /><stop offset="1" stopColor="#061F2C" /></linearGradient>
          <linearGradient id="hero-land" x1="382" y1="219" x2="579" y2="388" gradientUnits="userSpaceOnUse"><stop stopColor="#91FFD0" /><stop offset="1" stopColor="#1B9B78" /></linearGradient>
          <linearGradient id="hero-mountain" x1="472" y1="364" x2="554" y2="529" gradientUnits="userSpaceOnUse"><stop stopColor="#355C50" /><stop offset=".45" stopColor="#15372F" /><stop offset="1" stopColor="#071D19" /></linearGradient>
          <radialGradient id="hero-globe-glow"><stop stopColor="#41F2BE" stopOpacity=".38" /><stop offset="1" stopColor="#0ACB8B" stopOpacity="0" /></radialGradient>
          <clipPath id="hero-earth-clip"><circle cx="488" cy="303" r="127" /></clipPath>
          <filter id="hero-glow" x="-50%" y="-50%" width="200%" height="200%"><feGaussianBlur stdDeviation="8" result="blur" /><feMerge><feMergeNode in="blur" /><feMergeNode in="SourceGraphic" /></feMerge></filter>
        </defs>
        <circle cx="488" cy="303" r="172" fill="url(#hero-globe-glow)" />
        <g fill="#25D99A" fillOpacity=".3">
          <rect x="302" y="294" width="17" height="171" rx="3" /><rect x="329" y="267" width="17" height="198" rx="3" /><rect x="356" y="237" width="17" height="228" rx="3" />
          <rect x="383" y="257" width="17" height="208" rx="3" /><rect x="410" y="208" width="17" height="257" rx="3" /><rect x="437" y="192" width="17" height="273" rx="3" />
          <rect x="464" y="167" width="17" height="298" rx="3" /><rect x="491" y="183" width="17" height="282" rx="3" /><rect x="518" y="147" width="17" height="318" rx="3" />
          <rect x="545" y="131" width="17" height="334" rx="3" /><rect x="572" y="143" width="17" height="322" rx="3" /><rect x="599" y="101" width="17" height="364" rx="3" />
          <rect x="626" y="82" width="17" height="383" rx="3" /><rect x="653" y="96" width="17" height="369" rx="3" />
        </g>
        <circle cx="488" cy="303" r="127" fill="url(#hero-ocean)" stroke="#77FFD0" strokeOpacity=".78" strokeWidth="2" />
        <g clipPath="url(#hero-earth-clip)">
          <path d="m391 236 23-19 25 4 8 15 19 7-5 18-21 7-8 22-18 10-10 29-16 7-7-21-19-17 2-23 15-12 2-18 17-9 12-17Z" fill="url(#hero-land)" />
          <path d="m451 325 20-12 21 9 10 18-8 22-14 16-5 31-15 18-9-24-15-17-1-26-12-17 10-19 18-9Z" fill="url(#hero-land)" />
          <path d="m511 238 20-15 25 7 11 16 22 6 13 22-11 17-20-2-10 20-19-7-11-18-17-5-7-20-16-3-7-16 17-2Z" fill="url(#hero-land)" />
          <path d="m546 300 18-8 15 12 7 22-13 18-4 28-17 13-12-16 2-24-14-17 7-19Z" fill="url(#hero-land)" />
          <path d="M361 303h254M488 176v254M404 199c48 35 120 35 168 0M380 250c63 31 153 31 216 0M380 355c63-31 153-31 216 0M404 406c48-35 120-35 168 0" stroke="#9BFFE0" strokeOpacity=".25" />
          <ellipse cx="488" cy="303" rx="60" ry="127" stroke="#9BFFE0" strokeOpacity=".32" />
          <ellipse cx="488" cy="303" rx="104" ry="127" stroke="#9BFFE0" strokeOpacity=".19" />
        </g>
        <circle cx="488" cy="303" r="135" stroke="#A3FFE0" strokeOpacity=".3" />
        <path d="m108 434 95-63 63 18 76-82 60 15 95-89 55 10 107-130" stroke="url(#hero-line)" strokeWidth="8" strokeLinecap="round" strokeLinejoin="round" filter="url(#hero-glow)" />
        <path d="m680 106 29-33 10 45" stroke="#A2FFE2" strokeWidth="8" strokeLinecap="round" strokeLinejoin="round" filter="url(#hero-glow)" />
        <g fill="#A2FFE2" filter="url(#hero-glow)"><circle cx="203" cy="371" r="4" /><circle cx="408" cy="322" r="4" /><circle cx="552" cy="243" r="4" /></g>
        <path d="m168 506 103-71 55 20 85-65 55 34 80-78 49 24 72-56 93 47v179H168v-34Z" fill="url(#hero-mountain)" />
        <path d="m168 506 103-71 55 20 85-65 55 34 80-78 49 24 72-56 93 47" stroke="#75C9A4" strokeOpacity=".55" strokeWidth="2" />
        <path d="m271 435 26 39 29-19m85-65 29 44 26-10m80-78 29 41 20-17m72-39 25 35 22-14" stroke="#C1F4D9" strokeOpacity=".28" strokeWidth="2" />
      </svg>
      <div className="art-orbit orbit-one" /><div className="art-orbit orbit-two" /><div className="art-glow" />
    </div>
    <div className="login-panel">
      <div className="login-chart-decoration" aria-hidden="true">{Array.from({ length: 11 }, (_, index) => <i key={index} />)}</div>
      <div className="login-mobile-brand"><img src="/stocksathi-logo.png" alt="StockSathi" /></div>
      <div className="login-form-wrap">
        <div className="login-icon"><ShieldCheck size={19} /></div>
        <span className="eyebrow">WELCOME BACK</span>
        <h2>Sign in to your workspace</h2>
        <p className="login-subtitle">Enter your details to access your operations dashboard.</p>
        <form onSubmit={(event) => void submit(event)} className="login-form">
          <label>Email address<span className="login-input-wrap"><Mail size={15} /><input type="email" autoComplete="username" value={email} onChange={(event) => setEmail(event.target.value)} placeholder="you@company.com" required /></span></label>
          <label>Password<span className="login-input-wrap"><LockKeyhole size={15} /><input type="password" autoComplete="current-password" value={password} onChange={(event) => setPassword(event.target.value)} placeholder="Enter your password" required /></span></label>
          {error && <div className="form-error">{error}</div>}
          <button className="primary-button login-submit" disabled={busy}>{busy ? <><span className="button-spinner" /> Signing in...</> : <>Sign in <ArrowRight size={17} /></>}</button>
        </form>
        <p className="login-hint">Demo password: <code>ChangeMe123!</code></p>
      </div>
      <div className="login-legal"><span>© 2026 StockSathi</span><span><ShieldCheck size={14} /> Secure, role-based access</span></div>
    </div>
  </div>;
}

function DashboardView({ data, loading, onNavigate }: { data: Dashboard | null; loading: boolean; onNavigate: (section: Section) => void }) {
  const metrics = data?.metrics;
  return <div className="page-wrap">
    <div className="page-heading dashboard-heading"><div>    <div className="eyebrow">{new Date().toLocaleDateString("en-US", { weekday: "long", month: "long", day: "numeric", year: "numeric" }).toUpperCase()} <span className="heading-dot">·</span> OPERATIONS OVERVIEW</div><h1>Good morning<span className="heading-period">,</span> team <span className="wave">✳</span></h1><p>Here's what's happening across your supply chain today.</p></div><button className="secondary-button" onClick={() => onNavigate("reports")}><FileBarChart2 size={16} /> View reports</button></div>
    <div className="metrics-grid">
      <MetricCard title="Active products" value={metrics?.products} icon={Package} note={`${metrics?.lowStock || 0} need restocking`} tone="blue" loading={loading} />
      <MetricCard title="Warehouse stock" value={metrics?.totalUnits?.toLocaleString()} icon={Boxes} note={`Across ${metrics?.warehouses || 0} locations`} tone="green" loading={loading} />
      <MetricCard title="In transit" value={metrics?.inTransit} icon={Truck} note={`${metrics?.delayed || 0} shipments delayed`} tone="amber" loading={loading} />
      <MetricCard title="Open purchase orders" value={metrics?.openOrders} icon={ArrowDownLeft} note="Awaiting delivery" tone="violet" loading={loading} />
    </div>
    <div className="dashboard-grid">
      <section className="panel chart-panel"><div className="panel-heading"><div><h2>Inventory movement</h2><p>Units received and dispatched · last 7 days</p></div><button className="chart-filter">Last 7 days <ChevronDown size={14} /></button></div><div className="chart-legend"><span><i className="legend-swatch received" /> Received</span><span><i className="legend-swatch dispatched" /> Dispatched</span></div><div className="chart-area">{loading ? <Skeleton className="chart-skeleton" /> : <ResponsiveContainer width="100%" height="100%"><AreaChart data={data?.movementByDay || []} margin={{ top: 10, right: 4, left: -20, bottom: 0 }}><defs><linearGradient id="receivedFill" x1="0" y1="0" x2="0" y2="1"><stop offset="0%" stopColor="#258a72" stopOpacity={0.16} /><stop offset="95%" stopColor="#258a72" stopOpacity={0} /></linearGradient><linearGradient id="dispatchedFill" x1="0" y1="0" x2="0" y2="1"><stop offset="0%" stopColor="#99a8b9" stopOpacity={0.15} /><stop offset="95%" stopColor="#99a8b9" stopOpacity={0} /></linearGradient></defs><CartesianGrid stroke="#edf0f2" vertical={false} /><XAxis dataKey="day" axisLine={false} tickLine={false} tick={{ fill: "#919ba7", fontSize: 11, fontFamily: "DM Sans" }} dy={10} /><YAxis axisLine={false} tickLine={false} tick={{ fill: "#919ba7", fontSize: 11, fontFamily: "DM Sans" }} /><Tooltip contentStyle={{ borderRadius: 10, border: "1px solid #edf0f2", boxShadow: "0 8px 25px #26364212", fontSize: 12 }} /><Area type="monotone" dataKey="received" name="Received" stroke="#258a72" strokeWidth={2.2} fill="url(#receivedFill)" /><Area type="monotone" dataKey="dispatched" name="Dispatched" stroke="#99a8b9" strokeWidth={2} fill="url(#dispatchedFill)" /></AreaChart></ResponsiveContainer>}</div></section>
      <section className="panel alert-panel"><div className="panel-heading"><div><h2>Restock watch</h2><p>Products below their reorder level</p></div><button className="text-action" onClick={() => onNavigate("inventory")}>View inventory <ArrowRight size={14} /></button></div>{loading ? <Skeleton className="list-skeleton" /> : data?.lowStock.length ? <div className="low-stock-list">{data.lowStock.map((item) => <div className="low-stock-row" key={text(item.id)}><div className="product-avatar">{text(item.name, "?").slice(0, 1)}</div><div className="low-stock-info"><strong>{text(item.name)}</strong><small>{text(item.sku)} <span>·</span> {text(item.category)}</small></div><div className="stock-indicator"><strong>{number(item.quantity)}</strong><small>in stock</small></div></div>)}</div> : <EmptyState icon={Check} title="Stock looks healthy" text="No products are below their reorder level." />}</section>
    </div>
    <div className="dashboard-grid lower-grid">
      <section className="panel table-panel"><div className="panel-heading"><div><h2>Recent shipments</h2><p>A live view of goods on the move</p></div><button className="text-action" onClick={() => onNavigate("shipments")}>All shipments <ArrowRight size={14} /></button></div><div className="compact-table-wrap"><table className="data-table compact-table"><thead><tr><th>TRACKING</th><th>ROUTE</th><th>STATUS</th><th>ETA</th></tr></thead><tbody>{data?.recentShipments.map((item) => <tr key={text(item.id)}><td><strong>{text(item.trackingNumber)}</strong></td><td>{text((item.origin as Row | undefined)?.city)} <ArrowRight className="inline-arrow" size={12} /> {text((item.destination as Row | undefined)?.city)}</td><td><StatusBadge status={text(item.status)} /></td><td>{formatDate(item.expectedAt)}</td></tr>)}{!loading && !data?.recentShipments.length && <tr><td colSpan={4}><EmptyState icon={Truck} title="No shipments yet" text="Create a shipment to see it here." /></td></tr>}</tbody></table>{loading && <Skeleton className="table-skeleton" />}</div></section>
      <section className="panel activity-panel"><div className="panel-heading"><div><h2>Latest activity</h2><p>Recent changes across the network</p></div><button className="text-action" onClick={() => onNavigate("reports")}>View all <ArrowRight size={14} /></button></div>{loading ? <Skeleton className="list-skeleton" /> : <div className="activity-list">{data?.recentMovements.slice(0, 5).map((item) => { const positive = number(item.delta) > 0; return <div className="activity-row" key={text(item.id)}><span className={`activity-mark ${positive ? "activity-in" : "activity-out"}`}>{positive ? <ArrowDownLeft size={14} /> : <ArrowUpRightIcon />}</span><div className="activity-copy"><strong>{positive ? "Stock received" : "Stock dispatched"} <span>{text((item.product as Row | undefined)?.name)}</span></strong><small>{text((item.warehouse as Row | undefined)?.name)} · {text((item.user as Row | undefined)?.name)}</small></div><time>{formatRelative(item.createdAt)}</time></div>; })}</div>}</section>
    </div>
    <div className="dashboard-footnote"><span><Clock3 size={14} /> Data refreshes every time you visit a section.</span><button onClick={() => onNavigate("routes")}>Explore your network <ArrowRight size={14} /></button></div>
  </div>;
}

function ArrowUpRightIcon() {
  return <ArrowDownRight size={14} />;
}

function formatRelative(value: unknown) {
  if (!value) return "—";
  const minutes = Math.max(0, Math.floor((Date.now() - new Date(String(value)).getTime()) / 60000));
  return minutes < 1 ? "now" : minutes < 60 ? `${minutes}m` : minutes < 1440 ? `${Math.floor(minutes / 60)}h` : `${Math.floor(minutes / 1440)}d`;
}

function downloadStockCsv(rows: Row[]) {
  const columns: { heading: string; value: (row: Row) => string }[] = [
    { heading: "Product SKU", value: (row) => text((row.product as Row | undefined)?.sku, "") },
    { heading: "Product", value: (row) => text((row.product as Row | undefined)?.name, "") },
    { heading: "Warehouse", value: (row) => text((row.warehouse as Row | undefined)?.name, "") },
    { heading: "City", value: (row) => text((row.warehouse as Row | undefined)?.city, "") },
    { heading: "Quantity", value: (row) => text(row.quantity, "0") },
    { heading: "Unit", value: (row) => text((row.product as Row | undefined)?.unit, "") },
  ];
  const escape = (value: string) => `"${value.replaceAll('"', '""')}"`;
  const csv = [columns.map((column) => escape(column.heading)).join(","), ...rows.map((row) => columns.map((column) => escape(column.value(row))).join(","))].join("\r\n");
  const url = URL.createObjectURL(new Blob([csv], { type: "text/csv;charset=utf-8" }));
  const link = document.createElement("a");
  link.href = url;
  link.download = `stocksathi-stock-${new Date().toISOString().slice(0, 10)}.csv`;
  link.click();
  URL.revokeObjectURL(url);
}

function MetricCard({ title, value, icon: Icon, note, tone, loading }: { title: string; value?: string | number; icon: typeof Package; note: string; tone: string; loading: boolean }) {
  return <div className="metric-card"><div className={`metric-icon ${tone}`}><Icon size={18} strokeWidth={1.9} /></div><span className="metric-note">{note}</span><span className="metric-title">{title}</span><strong className="metric-value">{loading ? <Skeleton className="value-skeleton" /> : value ?? "—"}</strong></div>;
}

function ModuleView({ section, rows, loading, search, canWrite, canCreate, userRole, onCreate, onAction }: {
  section: Section; rows: Row[]; loading: boolean; search: string; canWrite: boolean; canCreate: boolean; userRole: string;
  onCreate: () => void; onAction: (row: Row, action: string) => void;
}) {
  const [query, setQuery] = useState("");
  const [selectedWarehouseId, setSelectedWarehouseId] = useState("");
  useEffect(() => setQuery(""), [section]);
  useEffect(() => setSelectedWarehouseId(""), [section]);
  const config: Record<Exclude<Section, "overview">, { title: string; subtitle: string; icon: typeof Package; create?: string; columns: { label: string; value: (row: Row) => React.ReactNode }[] }> = {
    inventory: { title: "Inventory", subtitle: "Product catalog, availability, and stock across your network.", icon: Package, create: "Add product", columns: [
      { label: "PRODUCT", value: (r) => <div className="product-cell"><span className="product-avatar">{text(r.name, "?").slice(0, 1)}</span><span><strong>{text(r.name)}</strong><small>{text(r.sku)} · {text(r.category)}</small></span></div> },
      { label: "TOTAL STOCK", value: (r) => <strong>{number(r.totalQuantity).toLocaleString()} <span className="unit-label">{text(r.unit)}</span></strong> },
      { label: "REORDER AT", value: (r) => text(r.reorderLevel) },
      { label: "STATUS", value: (r) => <StatusBadge status={number(r.totalQuantity) <= number(r.reorderLevel) ? "LOW STOCK" : "HEALTHY"} /> },
      { label: "LOCATIONS", value: (r) => <span className="location-list">{toRows(r.stock).filter((s) => number(s.quantity) > 0).map((s) => text((s.warehouse as Row | undefined)?.code)).join(", ") || "Not stocked"}</span> },
    ] },
    warehouses: { title: "Warehouses", subtitle: "Select a warehouse to see its complete on-hand inventory.", icon: Building2, create: "Add warehouse", columns: [
      { label: "LOCATION", value: (r) => <button type="button" className="warehouse-location-button" aria-expanded={selectedWarehouseId === text(r.id)} onClick={() => setSelectedWarehouseId(text(r.id))}><span className="product-cell"><span className="warehouse-avatar"><Building2 size={16} /></span><span><strong>{text(r.name)}</strong><small>{text(r.code)} · {text(r.address)}</small></span></span></button> },
      { label: "CITY", value: (r) => text(r.city) },
      { label: "ITEMS", value: (r) => <span className="stock-item-count">{toRows(r.stock).filter((item) => number(item.quantity) > 0).length}<small>on hand</small></span> },
      { label: "CAPACITY", value: (r) => <div className="capacity-cell"><span>{number(r.usedCapacity).toLocaleString()} <small>/ {number(r.capacity).toLocaleString()} units</small></span><span className="capacity-track"><i style={{ width: `${Math.min(100, Math.round(number(r.usedCapacity) / Math.max(1, number(r.capacity)) * 100))}%` }} /></span></div> },
      { label: "NETWORK", value: (r) => <span className="network-count"><Route size={14} /> {number((r._count as Row | undefined)?.outboundRoutes)} outbound lanes</span> },
    ] },
    suppliers: { title: "Suppliers", subtitle: "Manage your supplier relationships and contact information.", icon: Users, create: "Add supplier", columns: [
      { label: "SUPPLIER", value: (r) => <div className="product-cell"><span className="supplier-avatar">{text(r.name, "?").slice(0, 1)}</span><span><strong>{text(r.name)}</strong><small>{text(r.code)}</small></span></div> },
      { label: "CONTACT", value: (r) => <span>{text(r.email)}<small className="sub-cell">{text(r.phone)}</small></span> },
      { label: "LOCATION", value: (r) => text(r.address) },
      { label: "PURCHASE ORDERS", value: (r) => <span className="order-count">{number((r._count as Row | undefined)?.orders)}</span> },
    ] },
    orders: { title: "Purchase orders", subtitle: "Order inventory from suppliers and record incoming deliveries.", icon: Boxes, create: "New purchase order", columns: [
      { label: "ORDER", value: (r) => <div><strong>{text(r.orderNumber)}</strong><small className="sub-cell">{formatDate(r.createdAt)}</small></div> },
      { label: "SUPPLIER", value: (r) => text((r.supplier as Row | undefined)?.name) },
      { label: "DELIVER TO", value: (r) => text((r.warehouse as Row | undefined)?.name) },
      { label: "ITEMS", value: (r) => toRows(r.items).map((item) => `${text((item.product as Row | undefined)?.name)} · ${number(item.quantityReceived)}/${number(item.quantityOrdered)}`).join(", ") },
      { label: "EXPECTED", value: (r) => formatDate(r.expectedAt) },
      { label: "STATUS", value: (r) => <StatusBadge status={text(r.status)} /> },
    ] },
    routes: { title: "Route network", subtitle: "Active warehouse connections used to plan and validate shipments.", icon: Route, create: "Add shipping lane", columns: [
      { label: "FROM", value: (r) => <div><strong>{text((r.fromWarehouse as Row | undefined)?.name)}</strong><small className="sub-cell">{text((r.fromWarehouse as Row | undefined)?.city)}</small></div> },
      { label: "", value: () => <ArrowRight className="route-arrow" size={18} /> },
      { label: "TO", value: (r) => <div><strong>{text((r.toWarehouse as Row | undefined)?.name)}</strong><small className="sub-cell">{text((r.toWarehouse as Row | undefined)?.city)}</small></div> },
      { label: "DISTANCE", value: (r) => number(r.distanceKm) > 0 ? `${number(r.distanceKm).toLocaleString()} km` : "Not set" },
      { label: "TRANSIT TIME", value: (r) => `${number(r.transitHours)} hours` },
      { label: "STATUS", value: (r) => <StatusBadge status={r.active ? "ACTIVE" : "PAUSED"} /> },
    ] },
    shipments: { title: "Shipments", subtitle: "Track, advance, and manage goods moving across your network.", icon: Truck, create: "Create shipment", columns: [
      { label: "TRACKING", value: (r) => <div><strong>{text(r.trackingNumber)}</strong><small className="sub-cell">{formatDate(r.createdAt)}</small></div> },
      { label: "ROUTE", value: (r) => <span>{text((r.origin as Row | undefined)?.city)} <ArrowRight className="inline-arrow" size={12} /> {text((r.destination as Row | undefined)?.city)}</span> },
      { label: "CARGO", value: (r) => toRows(r.items).map((item) => `${number(item.quantity)} × ${text((item.product as Row | undefined)?.name)}`).join(", ") },
      { label: "ETA", value: (r) => formatDate(r.expectedAt) },
      { label: "STATUS", value: (r) => <StatusBadge status={text(r.status)} /> },
    ] },
    reports: { title: "Inventory report", subtitle: "Current stock by product and warehouse location.", icon: FileBarChart2, columns: [
      { label: "PRODUCT", value: (r) => <div><strong>{text((r.product as Row | undefined)?.name)}</strong><small className="sub-cell">{text((r.product as Row | undefined)?.sku)}</small></div> },
      { label: "WAREHOUSE", value: (r) => text((r.warehouse as Row | undefined)?.name) },
      { label: "CITY", value: (r) => text((r.warehouse as Row | undefined)?.city) },
      { label: "ON HAND", value: (r) => <strong>{number(r.quantity).toLocaleString()} <span className="unit-label">{text((r.product as Row | undefined)?.unit)}</span></strong> },
      { label: "REORDER LEVEL", value: (r) => text((r.product as Row | undefined)?.reorderLevel) },
    ] },
    team: { title: "Team access", subtitle: "Manage your workspace members and access levels.", icon: ShieldCheck, create: "Invite team member", columns: [
      { label: "TEAM MEMBER", value: (r) => <div className="product-cell"><span className="avatar table-avatar">{initials(text(r.name))}</span><span><strong>{text(r.name)}</strong><small>{text(r.email)}</small></span></div> },
      { label: "ROLE", value: (r) => <span className="role-pill">{text(r.role).toLowerCase()}</span> },
      { label: "STATUS", value: (r) => <StatusBadge status={r.active ? "ACTIVE" : "INACTIVE"} /> },
      { label: "JOINED", value: (r) => formatDate(r.createdAt) },
    ] },
    activity: { title: "Audit activity", subtitle: "A record of operational changes made across your workspace.", icon: Activity, columns: [
      { label: "ACTION", value: (r) => <span><strong>{text(r.action)}</strong><small className="sub-cell">{text(r.entity)} · {text(r.details)}</small></span> },
      { label: "CHANGED BY", value: (r) => text((r.user as Row | undefined)?.name) },
      { label: "TIME", value: (r) => formatTime(r.createdAt) },
    ] },
  };
  const current = config[section as Exclude<Section, "overview">];
  const filterText = [query, search].filter(Boolean).join(" ").toLowerCase();
  const filtered = rows.filter((row) => JSON.stringify(row).toLowerCase().includes(filterText));
  const selectedWarehouse = section === "warehouses" ? rows.find((row) => text(row.id) === selectedWarehouseId) : undefined;
  const selectedStockItems = toRows(selectedWarehouse?.stock).filter((item) => number(item.quantity) > 0);
  const Icon = current.icon;
  const title = section === "inventory" && search ? `${current.title}` : current.title;
  return <div className="page-wrap">
    <div className="page-heading module-heading"><div><div className="eyebrow"><Icon size={13} /> OPERATIONS</div><h1>{title}</h1><p>{current.subtitle}</p></div>{current.create && canCreate && <button className="primary-button" onClick={onCreate}><Plus size={17} /> {current.create}</button>}</div>
    <div className="module-summary-row"><div className="module-summary"><span className="summary-icon"><Icon size={17} /></span><span><strong>{loading ? "—" : filtered.length}</strong><small>{filtered.length === 1 ? "record" : "records"} in this view</small></span></div>{section === "shipments" && <div className="summary-caption"><span className="online-dot" /> Changes are recorded in the shipment timeline</div>}{section === "inventory" && <div className="summary-caption"><ArrowLeftRight size={15} /> Stock levels are updated through audited movements</div>}</div>
    <section className="panel module-panel"><div className="module-toolbar"><div className="table-heading"><div><h2>{section === "reports" ? "Stock by location" : current.title}</h2><span>{section === "inventory" ? "Catalog & stock" : section === "shipments" ? "Live tracking" : "Updated just now"}</span></div></div><div className="table-tools">{section === "inventory" && <label className="table-search"><Search size={15} /><input value={query} onChange={(event) => setQuery(event.target.value)} placeholder="Find a product..." /><kbd>/</kbd></label>}{section !== "inventory" && <label className="table-search"><Search size={15} /><input value={query} onChange={(event) => setQuery(event.target.value)} placeholder="Filter list..." /></label>}{section === "inventory" && canWrite && <button className="secondary-button table-action" onClick={() => onAction({}, "adjust-all")}><ArrowLeftRight size={15} /> Stock movement</button>}{section === "reports" && <button className="secondary-button table-action" onClick={() => downloadStockCsv(filtered)}><Download size={15} /> Export CSV</button>}</div></div>
      <div className="responsive-table"><table className="data-table"><thead><tr>{current.columns.map((column, index) => <th key={`${column.label}-${index}`}>{column.label}</th>)}{((["inventory", "warehouses", "suppliers", "team", "routes"].includes(section) && canWrite) || (section === "orders" && canWrite) || section === "shipments") && <th><span className="visually-hidden">ACTIONS</span></th>}</tr></thead><tbody>{filtered.map((row) => {
        const showActions = (["inventory", "warehouses", "suppliers", "team", "routes"].includes(section) && canWrite) || (section === "orders" && canWrite && ["OPEN", "PARTIAL"].includes(text(row.status))) || section === "shipments";
        return <tr key={text(row.id)}>{current.columns.map((column, index) => <td key={`${column.label}-${index}`}>{column.value(row)}</td>)}{showActions && <td className="row-actions">{section === "inventory" ? <div className="action-pair"><button className="small-action" onClick={() => onAction(row, "edit")}><Settings2 size={14} /> Edit</button><button className="small-action" onClick={() => onAction(row, "adjust")}><ArrowLeftRight size={14} /> Adjust</button></div> : section === "warehouses" || section === "suppliers" || section === "team" || section === "routes" ? <button className="small-action" onClick={() => onAction(row, "edit")}><Settings2 size={14} /> Manage</button> : section === "orders" ? <div className="action-pair"><button className="small-action receive-action" onClick={() => onAction(row, "receive")}><ArrowDownLeft size={14} /> Receive</button>{["ADMIN", "PROCUREMENT"].includes(userRole) && <button className="icon-button delay-button" onClick={() => onAction(row, "cancel")} title="Cancel purchase order"><X size={14} /></button>}</div> : <div className="action-pair"><button className="small-action" onClick={() => onAction(row, "track")}><MapPin size={13} /> Track</button>{canWrite && text(row.status) !== "DELIVERED" && <><button className="small-action" onClick={() => onAction(row, "advance")}><ArrowRight size={14} /> Advance</button><button className="icon-button delay-button" onClick={() => onAction(row, "delay")} title="Flag a delay"><Clock3 size={15} /></button></>}</div>}</td>}</tr>;
      })}{!loading && !filtered.length && <tr><td colSpan={current.columns.length + 1}><EmptyState icon={Icon} title={filterText ? "No matching records" : `No ${current.title.toLowerCase()} yet`} text={filterText ? "Try a different search term." : current.create ? "Add your first record to get started." : "Your data will appear here as you use the system."} /></td></tr>}</tbody></table>{loading && <div className="table-loading"><Skeleton className="table-skeleton" /><Skeleton className="table-skeleton" /><Skeleton className="table-skeleton" /></div>}</div>
      <div className="table-footer"><span>Showing <strong>{loading ? "—" : filtered.length}</strong> of <strong>{rows.length}</strong> records</span><span className="table-page">Page 1 <ChevronDown size={13} /></span></div>
    </section>
    {selectedWarehouse && <section className="panel warehouse-detail" id="warehouse-stock-detail">
      <div className="panel-heading warehouse-detail-heading"><div><h2>{text(selectedWarehouse.name)} on-hand items</h2><p>{text(selectedWarehouse.code)} · {selectedStockItems.length} {selectedStockItems.length === 1 ? "item" : "items"} in stock</p></div><button className="secondary-button" onClick={() => setSelectedWarehouseId("")}>Close</button></div>
      {selectedStockItems.length ? <div className="responsive-table"><table className="data-table stock-detail-table"><thead><tr><th>PRODUCT</th><th>SKU</th><th>ON HAND</th></tr></thead><tbody>{selectedStockItems.map((item) => {
        const product = item.product as Row | undefined;
        return <tr key={text(item.productId)}><td><strong>{text(product?.name)}</strong></td><td>{text(product?.sku)}</td><td><strong>{number(item.quantity).toLocaleString()} <span className="unit-label">{text(product?.unit, "pcs")}</span></strong></td></tr>;
      })}</tbody></table></div> : <EmptyState icon={Package} title="No items on hand" text="This warehouse has no recorded stock." />}
    </section>}
    {section === "orders" && <div className="report-note"><ArrowDownLeft size={16} /><span>Receiving an order adds the delivered quantity to the warehouse selected on that order. Partial deliveries are supported.</span></div>}
    {section === "routes" && <div className="report-note"><Route size={16} /><span>“Not set” means this route has no recorded distance. Use <strong>Manage</strong> to enter the actual lane distance and transit time.</span></div>}
    {section === "shipments" && <div className="shipment-help"><div className="shipment-help-icon"><Truck size={18} /></div><span><strong>How shipment tracking works</strong><small>Advance a shipment to its next stop. When it reaches its destination, inventory is received automatically.</small></span><ArrowRight size={16} /></div>}
    {section === "reports" && <div className="report-note"><FileBarChart2 size={16} /><span>Inventory is calculated from recorded movements. Use <strong>Stock movement</strong> in Inventory to keep quantities accurate.</span></div>}
    {section === "team" && userRole === "ADMIN" && <div className="report-note"><ShieldCheck size={16} /><span>For security, invite only teammates who need access. Roles determine which actions they can take.</span></div>}
  </div>;
}

function StatusBadge({ status }: { status: string }) {
  const key = status.toLowerCase().replaceAll(" ", "_");
  return <span className={`status-badge status-${key}`}><i />{status.replaceAll("_", " ").toLowerCase()}</span>;
}

function EmptyState({ icon: Icon, title, text: description }: { icon: typeof Package; title: string; text: string }) {
  return <div className="empty-state"><span><Icon size={20} /></span><strong>{title}</strong><small>{description}</small></div>;
}

function TrackingModal({ shipment, onClose }: { shipment: Row; onClose: () => void }) {
  const [warehouses, setWarehouses] = useState<Row[]>([]);
  const [error, setError] = useState("");
  const events = toRows(shipment.events);
  useEffect(() => {
    request<unknown>("/warehouses").then((value) => setWarehouses(toRows(value))).catch((reason: unknown) => setError(reason instanceof Error ? reason.message : "Could not load route details."));
  }, []);
  const byId = new Map(warehouses.map((warehouse) => [text(warehouse.id), warehouse]));
  let routeIds: string[] = [];
  let routeError = "";
  try {
    const routePath = shipment.routePath;
    const parsed: unknown = Array.isArray(routePath)
      ? routePath
      : JSON.parse(text(routePath, "[]"));
    if (Array.isArray(parsed) && parsed.every((item) => typeof item === "string")) routeIds = parsed;
  } catch {
    routeError = "This shipment has invalid route data.";
  }
  const currentLeg = number(shipment.currentLeg);
  const delivered = text(shipment.status) === "DELIVERED";
  const stops = routeIds.map((id, index) => ({
    id,
    label: text(byId.get(id)?.name, index === 0 ? text((shipment.origin as Row | undefined)?.name) : index === routeIds.length - 1 ? text((shipment.destination as Row | undefined)?.name) : `Route stop ${index + 1}`),
    city: text(byId.get(id)?.city, index === 0 ? text((shipment.origin as Row | undefined)?.city) : index === routeIds.length - 1 ? text((shipment.destination as Row | undefined)?.city) : ""),
  }));
  const closeOnEscape = (event: KeyboardEvent) => { if (event.key === "Escape") onClose(); };
  useEffect(() => {
    window.addEventListener("keydown", closeOnEscape);
    return () => window.removeEventListener("keydown", closeOnEscape);
  });
  return <div className="modal-backdrop" onMouseDown={(event) => { if (event.target === event.currentTarget) onClose(); }}><section className="modal-card tracking-card" role="dialog" aria-modal="true" aria-labelledby="tracking-title">
    <div className="modal-heading"><div><span className="eyebrow"><Truck size={13} /> SHIPMENT TRACKING</span><h2 id="tracking-title">{text(shipment.trackingNumber)}</h2><p>{text((shipment.origin as Row | undefined)?.name)} <ArrowRight className="inline-arrow" size={13} /> {text((shipment.destination as Row | undefined)?.name)}</p></div><button className="icon-button modal-close" onClick={onClose} aria-label="Close tracking"><X size={19} /></button></div>
    <div className="tracking-summary"><span><StatusBadge status={text(shipment.status)} /></span><span><Clock3 size={14} /> Expected {formatDate(shipment.expectedAt)}</span><span><Package size={14} /> {toRows(shipment.items).reduce((sum, item) => sum + number(item.quantity), 0)} units</span></div>
    {(error || routeError) && <div className="form-error">{error || routeError}</div>}
    <div className="tracking-section"><div className="tracking-section-title"><MapPin size={15} /><strong>Journey</strong><small>{delivered ? Math.max(0, routeIds.length - 1) : currentLeg} of {Math.max(0, routeIds.length - 1)} legs completed</small></div><div className="route-stops">{stops.map((stop, index) => {
      const complete = delivered || index < currentLeg;
      const current = !delivered && index === currentLeg;
      return <div className={`route-stop ${complete ? "stop-complete" : ""} ${current ? "stop-current" : ""}`} key={`${stop.id}-${index}`}><span className="stop-marker">{complete ? <Check size={12} /> : index + 1}</span><span className="stop-name"><strong>{stop.label}</strong><small>{stop.city}</small></span>{index < stops.length - 1 && <span className={`stop-line ${complete ? "line-complete" : ""}`} />}</div>;
    })}{!stops.length && <span className="tracking-empty">Route details are unavailable.</span>}</div></div>
    <div className="tracking-section events-section"><div className="tracking-section-title"><Activity size={15} /><strong>Updates</strong><small>Most recent first</small></div>{events.length ? <div className="tracking-events">{events.map((event) => <div className="tracking-event" key={text(event.id)}><span className="event-dot" /><div><strong>{text(event.status).replaceAll("_", " ").toLowerCase()}</strong><small>{text(event.note)}</small><time>{formatTime(event.createdAt)}</time></div></div>)}</div> : <p className="tracking-empty">No shipment updates recorded yet.</p>}</div>
    <div className="modal-actions"><button className="secondary-button" onClick={onClose}>Close</button></div>
  </section></div>;
}

function Skeleton({ className = "" }: { className?: string }) {
  return <span className={`skeleton ${className}`} />;
}

function Modal({ modal, onClose, onSubmit }: { modal: ModalState; onClose: () => void; onSubmit: (data: Record<string, string>) => Promise<void> }) {
  const [values, setValues] = useState<Record<string, string>>(modal.values || {});
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState("");
  useEffect(() => {
    const handleKey = (event: KeyboardEvent) => { if (event.key === "Escape" && !busy) onClose(); };
    window.addEventListener("keydown", handleKey);
    return () => window.removeEventListener("keydown", handleKey);
  }, [busy, onClose]);
  async function submit(event: React.FormEvent) {
    event.preventDefault();
    setBusy(true);
    setError("");
    try { await onSubmit(values); } catch (reason) { setError(reason instanceof Error ? reason.message : "Could not save these changes."); }
    finally { setBusy(false); }
  }
  return <div className="modal-backdrop" onMouseDown={(event) => { if (event.target === event.currentTarget && !busy) onClose(); }}><section className="modal-card" role="dialog" aria-modal="true" aria-labelledby="modal-title"><div className="modal-heading"><div><span className="eyebrow">NORTHSTAR WORKSPACE</span><h2 id="modal-title">{modal.title}</h2><p>{modal.description}</p></div><button className="icon-button modal-close" onClick={onClose} aria-label="Close"><X size={19} /></button></div><form onSubmit={(event) => void submit(event)}>  <div className="modal-fields">{modal.fields.map((field) => {
    const options = field.optionsFor ? field.optionsFor(values) : field.options;
    return <label className={field.wide ? "field-wide" : ""} key={field.name}>{field.label}{options ? <select required={field.required} value={values[field.name] || ""} onChange={(event) => {
      const nextValues = { ...values, [field.name]: event.target.value };
      for (const name of field.clearOnChange || []) delete nextValues[name];
      setValues(nextValues);
    }}><option value="">{options.length ? "Select an option" : field.emptyLabel?.(values) || "Select an option"}</option>{options.map((option) => <option key={option.value} value={option.value}>{option.label}</option>)}</select> : field.type === "textarea" ? <textarea rows={3} required={field.required} value={values[field.name] || ""} onChange={(event) => setValues({ ...values, [field.name]: event.target.value })} /> : <input type={field.type || "text"} min={field.type === "number" ? field.min ?? 1 : undefined} step={field.type === "number" ? 1 : undefined} required={field.required} value={values[field.name] || ""} onChange={(event) => setValues({ ...values, [field.name]: event.target.value })} />}</label>;
  })}</div>{error && <div className="form-error modal-error">{error}</div>}<div className="modal-actions"><button type="button" className="secondary-button" onClick={onClose} disabled={busy}>Cancel</button><button type="submit" className="primary-button" disabled={busy}>{busy ? <><span className="button-spinner" /> Saving...</> : <><Check size={16} /> Save changes</>}</button></div></form></section></div>;
}
