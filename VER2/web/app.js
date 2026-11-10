let state = { products: [], warehouses: [], routes: [], shipments: [], suppliers: [] };
let busy = false;

const titles = {
    overview: "Overview",
    inventory: "Inventory",
    warehouses: "Warehouses",
    supply: "Supply chain",
    suppliers: "Suppliers",
    reports: "Reports",
    audit: "Audit log"
};

function el(tag, className, text) {
    const node = document.createElement(tag);
    if (className) node.className = className;
    if (text != null) node.textContent = text;
    return node;
}

function money(value) {
    return "$" + Number(value).toFixed(2);
}

function availableOf(product) {
    return product.quantity - product.stored - product.inTransit;
}

function warehouseById(id) {
    return state.warehouses.find((warehouse) => warehouse.id === Number(id));
}

function showBanner(ok, message) {
    const banner = document.getElementById("banner");
    if (!message) {
        banner.hidden = true;
        return;
    }
    banner.hidden = false;
    banner.className = ok ? "banner ok" : "banner bad";
    banner.textContent = message;
}

function setConnection(mode, text) {
    const pill = document.getElementById("connection");
    pill.className = "pill " + mode;
    pill.textContent = text;
}

async function api(path, payload) {
    if (busy) return { ok: false, message: "Please wait for the current action to finish." };
    busy = true;
    try {
        const response = await fetch(path, payload == null ? undefined : {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(payload)
        });
        const data = await response.json();
        if (data.state) {
            state = data.state;
            render();
        }
        if (data.message) showBanner(Boolean(data.ok), data.message);
        setConnection("live", "Connected");
        return data;
    } catch (error) {
        setConnection("down", "Offline");
        showBanner(false, "The local system is not responding. Start it from run_project.bat and leave that window open.");
        return { ok: false, message: error.message };
    } finally {
        busy = false;
    }
}

function fillSelect(select, options, placeholder) {
    const current = select.value;
    select.replaceChildren();
    select.appendChild(new Option(placeholder, ""));
    options.forEach((option) => select.appendChild(new Option(option.label, String(option.value))));
    if ([...select.options].some((option) => option.value === current)) select.value = current;
}

function warehouseOptions() {
    return state.warehouses.map((warehouse) => ({
        value: warehouse.id,
        label: warehouse.name + " [" + warehouse.id + "]"
    }));
}

function productOptions() {
    return state.products.map((product) => ({
        value: product.id,
        label: product.name + " [" + product.id + "]"
    }));
}

function renderStats() {
    const active = state.shipments.filter((shipment) => shipment.status !== "Delivered").length;
    const low = state.products.filter((product) => product.quantity < 10).length;
    const cards = [
        ["Products", state.products.length],
        ["Warehouses", state.warehouses.length],
        ["Routes", state.routes.length],
        ["In transit", active],
        ["Low stock", low],
        ["Suppliers", state.suppliers.length]
    ];
    const host = document.getElementById("stats");
    host.replaceChildren();
    cards.forEach(([label, value]) => {
        const card = el("article", "stat");
        card.append(el("span", "", label), el("strong", "", String(value)));
        host.appendChild(card);
    });
}

function renderLowStock() {
    const host = document.getElementById("low-stock");
    const items = state.products.filter((product) => product.quantity < 10);
    host.replaceChildren();
    if (!items.length) {
        host.appendChild(el("p", "empty", "No products are below 10 units."));
        return;
    }
    items.forEach((product) => {
        host.appendChild(el("p", "product-line", product.name + " [" + product.id + "] has " + product.quantity + " in the catalog."));
    });
}

function renderActiveShipments() {
    const host = document.getElementById("active-shipments");
    const items = state.shipments.filter((shipment) => shipment.status !== "Delivered");
    host.replaceChildren();
    if (!items.length) {
        host.appendChild(el("p", "empty", "No shipments are currently moving."));
        return;
    }
    items.forEach((shipment) => {
        const where = shipment.path.find((stop) => stop.current);
        host.appendChild(el("p", "product-line",
            "Shipment " + shipment.id + " · " + shipment.productName + " × " + shipment.quantity +
            " · " + shipment.status + (where ? " · at " + where.name : "")));
    });
}

function renderProducts() {
    const body = document.getElementById("product-rows");
    body.replaceChildren();
    if (!state.products.length) {
        const row = el("tr");
        const cell = el("td", "", "No products have been added yet.");
        cell.colSpan = 9;
        row.appendChild(cell);
        body.appendChild(row);
        return;
    }
    state.products.forEach((product) => {
        const row = el("tr");
        [product.id, product.name, money(product.price), product.quantity, product.stored, product.inTransit, availableOf(product)]
            .forEach((value) => row.appendChild(el("td", "", String(value))));
        const stockCell = el("td");
        const controls = el("div", "inline");
        const input = el("input");
        input.type = "number";
        input.min = "0";
        input.value = String(product.quantity);
        input.dataset.id = String(product.id);
        const update = el("button", "mini", "Update");
        update.type = "button";
        update.dataset.action = "stock";
        update.dataset.id = String(product.id);
        controls.append(input, update);
        stockCell.appendChild(controls);
        const actionCell = el("td");
        const remove = el("button", "mini danger", "Delete");
        remove.type = "button";
        remove.dataset.action = "delete";
        remove.dataset.id = String(product.id);
        actionCell.appendChild(remove);
        row.append(stockCell, actionCell);
        body.appendChild(row);
    });
}

function renderWarehouses() {
    fillSelect(document.getElementById("store-warehouse"), warehouseOptions(), "Select warehouse");
    fillSelect(document.getElementById("remove-warehouse"), warehouseOptions(), "Select warehouse");
    fillSelect(document.getElementById("store-product"), productOptions(), "Select product");
    fillSelect(document.getElementById("remove-product"), productOptions(), "Select product");
    fillSelect(document.getElementById("sku-warehouse"), warehouseOptions(), "Warehouse");
    fillSelect(document.getElementById("sku-product"), productOptions(), "Product");

    const query = document.getElementById("warehouse-search").value.trim().toLowerCase();
    const host = document.getElementById("warehouse-cards");
    host.replaceChildren();
    const visible = state.warehouses.filter((warehouse) => {
        const haystack = (warehouse.name + " " + warehouse.location).toLowerCase();
        return !query || haystack.includes(query);
    });
    if (!visible.length) {
        host.appendChild(el("p", "empty", state.warehouses.length ? "No warehouses match that search." : "No warehouses have been added."));
        return;
    }
    visible.forEach((warehouse) => {
        const card = el("article", "warehouse-card");
        const top = el("div", "card-top");
        top.appendChild(el("strong", "", warehouse.name + " [" + warehouse.id + "]"));
        const remove = el("button", "mini danger", "Delete");
        remove.type = "button";
        remove.dataset.action = "delete-warehouse";
        remove.dataset.id = String(warehouse.id);
        top.appendChild(remove);
        card.append(top, el("p", "muted", warehouse.location));
        const bar = el("div", "bar");
        const fill = el("span");
        const ratio = warehouse.capacity ? Math.min(100, Math.round((warehouse.used / warehouse.capacity) * 100)) : 0;
        fill.style.width = ratio + "%";
        bar.appendChild(fill);
        card.append(bar, el("p", "", warehouse.used + " / " + warehouse.capacity + " units used"));
        const products = [...warehouse.products].sort((a, b) => b.quantity - a.quantity);
        if (!products.length) card.appendChild(el("p", "empty", "No products stored here."));
        products.forEach((product) => {
            card.appendChild(el("p", "product-line", product.name + " [" + product.id + "] · " + product.quantity));
        });
        host.appendChild(card);
    });
    renderSku();
}

function renderSku() {
    const warehouse = warehouseById(document.getElementById("sku-warehouse").value);
    const productId = Number(document.getElementById("sku-product").value);
    const result = document.getElementById("sku-result");
    if (!warehouse || !productId) {
        result.textContent = "Choose a warehouse and product to see the aisle location.";
        return;
    }
    const stored = warehouse.products.find((product) => product.id === productId);
    const aisle = (productId % 8) + 1;
    const section = (warehouse.id % 5) + 1;
    const shelf = (Math.floor(productId / 10) % 6) + 1;
    result.textContent = warehouse.name + " · product " + productId +
        " · Aisle A" + aisle + ", Section S" + section + ", Shelf " + shelf +
        " · Stock here: " + (stored ? stored.quantity : 0) + " unit(s)";
}

function renderSupply() {
    const warehouses = warehouseOptions();
    ["route-from", "route-to", "find-from", "find-to", "ship-from", "ship-to"].forEach((id) => {
        fillSelect(document.getElementById(id), warehouses, "Select warehouse");
    });
    fillSelect(document.getElementById("ship-product"), productOptions(), "Select product");

    const routes = document.getElementById("route-list");
    routes.replaceChildren();
    if (!state.routes.length) {
        routes.appendChild(el("p", "empty", "No routes have been added yet."));
    } else {
        state.routes.forEach((route) => {
            const from = warehouseById(route.from);
            const to = warehouseById(route.to);
            routes.appendChild(el("p", "route-line",
                (from ? from.name : "Warehouse") + " [" + route.from + "]  —  " +
                (to ? to.name : "Warehouse") + " [" + route.to + "]"));
        });
    }

    const list = document.getElementById("shipment-list");
    list.replaceChildren();
    if (!state.shipments.length) {
        list.appendChild(el("p", "empty", "No shipments have been created yet."));
        return;
    }
    state.shipments.forEach((shipment) => {
        const card = el("article", "shipment");
        const top = el("div", "card-top");
        top.appendChild(el("strong", "", "Shipment " + shipment.id + " · " + shipment.productName + " × " + shipment.quantity));
        const chip = el("span", "chip " + shipment.status.toLowerCase().replace(" ", ""), shipment.status);
        top.appendChild(chip);
        const steps = el("div", "steps");
        shipment.path.forEach((stop) => {
            steps.appendChild(el("span", stop.current ? "step current" : "step", stop.name + " [" + stop.id + "]"));
        });
        card.append(top, steps);
        if (shipment.status !== "Delivered") {
            const actions = el("div", "row-actions");
            if (shipment.status !== "Delayed") {
                const advance = el("button", "mini", "Advance one step");
                advance.type = "button";
                advance.dataset.action = "advance";
                advance.dataset.id = String(shipment.id);
                actions.appendChild(advance);
                const delay = el("button", "mini ghost", "Mark delayed");
                delay.type = "button";
                delay.dataset.action = "delay";
                delay.dataset.id = String(shipment.id);
                actions.appendChild(delay);
            } else {
                const resume = el("button", "mini", "Resume");
                resume.type = "button";
                resume.dataset.action = "resume";
                resume.dataset.id = String(shipment.id);
                actions.appendChild(resume);
            }
            card.appendChild(actions);
        }
        list.appendChild(card);
    });
}

function renderSuppliers() {
    const host = document.getElementById("supplier-list");
    host.replaceChildren();
    if (!state.suppliers.length) {
        host.appendChild(el("p", "empty", "No suppliers registered."));
        return;
    }
    state.suppliers.forEach((supplier) => {
        const card = el("article", "supplier-card");
        const top = el("div", "card-top");
        top.appendChild(el("strong", "", supplier.name + " [" + supplier.id + "]"));
        const remove = el("button", "mini danger", "Delete");
        remove.type = "button";
        remove.dataset.action = "delete-supplier";
        remove.dataset.id = String(supplier.id);
        top.appendChild(remove);
        card.appendChild(top);
        card.appendChild(el("p", "", supplier.contact));
        const rating = el("div", "rating");
        const fill = el("span");
        fill.style.width = (Math.max(0, Math.min(5, supplier.rating)) / 5 * 100) + "%";
        rating.appendChild(fill);
        card.append(el("p", "muted", "Rating " + Number(supplier.rating).toFixed(1) + " / 5"), rating);
        const form = el("form", "form-stack");
        const contact = el("input");
        contact.value = supplier.contact;
        contact.required = true;
        const score = el("input");
        score.type = "number";
        score.min = "0";
        score.max = "5";
        score.step = "0.1";
        score.value = String(supplier.rating);
        const save = el("button", "", "Update supplier");
        save.type = "submit";
        form.append(contact, score, save);
        form.addEventListener("submit", (event) => {
            event.preventDefault();
            api("/api/supplier/update", {
                id: supplier.id,
                contact: contact.value.trim(),
                rating: Number(score.value)
            });
        });
        card.appendChild(form);
        host.appendChild(card);
    });
}

function render() {
    renderStats();
    renderLowStock();
    renderActiveShipments();
    renderProducts();
    renderWarehouses();
    renderSupply();
    renderSuppliers();
}

function showView(name) {
    document.querySelectorAll(".dock button[data-view]").forEach((button) => {
        button.classList.toggle("active", button.dataset.view === name);
    });
    document.querySelectorAll("main section").forEach((section) => {
        section.hidden = section.id !== "view-" + name;
    });
    document.getElementById("view-title").textContent = titles[name];
    if (name === "audit") loadAudit();
}

async function loadAudit() {
    try {
        const response = await fetch("/api/audit");
        const data = await response.json();
        const list = document.getElementById("audit-list");
        list.replaceChildren();
        const lines = data.lines || [];
        if (!lines.length) {
            list.appendChild(el("li", "", "The audit log is empty."));
            return;
        }
        lines.slice().reverse().forEach((line) => list.appendChild(el("li", "", line)));
    } catch (error) {
        showBanner(false, "The audit log could not be read.");
    }
}

function numberValue(id) {
    return Number(document.getElementById(id).value);
}

document.querySelectorAll(".dock button[data-view], #quick-actions button[data-view]").forEach((button) => {
    button.addEventListener("click", () => showView(button.dataset.view));
});

document.getElementById("refresh").addEventListener("click", () => api("/api/state"));
document.getElementById("reload-audit").addEventListener("click", loadAudit);
document.getElementById("warehouse-search").addEventListener("input", renderWarehouses);
document.getElementById("sku-warehouse").addEventListener("change", renderSku);
document.getElementById("sku-product").addEventListener("change", renderSku);

document.getElementById("product-form").addEventListener("submit", (event) => {
    event.preventDefault();
    api("/api/inventory/add", {
        id: numberValue("product-id"),
        name: document.getElementById("product-name").value.trim(),
        price: numberValue("product-price"),
        quantity: numberValue("product-qty")
    });
});

document.getElementById("product-rows").addEventListener("click", (event) => {
    const button = event.target.closest("button");
    if (!button) return;
    const id = Number(button.dataset.id);
    if (button.dataset.action === "delete") {
        if (confirm("Delete product " + id + "?")) api("/api/inventory/delete", { id });
        return;
    }
    const input = document.querySelector("#product-rows input[data-id='" + id + "']");
    api("/api/inventory/stock", { id, quantity: Number(input.value) });
});

document.getElementById("warehouse-form").addEventListener("submit", (event) => {
    event.preventDefault();
    api("/api/warehouse/add", {
        id: numberValue("warehouse-id"),
        name: document.getElementById("warehouse-name").value.trim(),
        location: document.getElementById("warehouse-location").value.trim(),
        capacity: numberValue("warehouse-capacity")
    });
});

document.getElementById("store-form").addEventListener("submit", (event) => {
    event.preventDefault();
    api("/api/warehouse/store", {
        warehouseId: numberValue("store-warehouse"),
        productId: numberValue("store-product"),
        quantity: numberValue("store-qty")
    });
});

document.getElementById("remove-form").addEventListener("submit", (event) => {
    event.preventDefault();
    api("/api/warehouse/remove", {
        warehouseId: numberValue("remove-warehouse"),
        productId: numberValue("remove-product"),
        quantity: numberValue("remove-qty")
    });
});

document.getElementById("warehouse-cards").addEventListener("click", (event) => {
    const button = event.target.closest("button");
    if (!button || button.dataset.action !== "delete-warehouse") return;
    const id = Number(button.dataset.id);
    if (confirm("Delete warehouse " + id + "?")) api("/api/warehouse/delete", { id });
});

document.getElementById("route-form").addEventListener("submit", (event) => {
    event.preventDefault();
    api("/api/route/add", { from: numberValue("route-from"), to: numberValue("route-to") });
});

document.getElementById("remove-route").addEventListener("click", () => {
    api("/api/route/remove", { from: numberValue("route-from"), to: numberValue("route-to") });
});

document.getElementById("find-form").addEventListener("submit", async (event) => {
    event.preventDefault();
    const data = await api("/api/route/find", { from: numberValue("find-from"), to: numberValue("find-to") });
    if (data && data.message) document.getElementById("route-result").textContent = data.message;
});

document.getElementById("shipment-form").addEventListener("submit", (event) => {
    event.preventDefault();
    api("/api/shipment/create", {
        originId: numberValue("ship-from"),
        destinationId: numberValue("ship-to"),
        productId: numberValue("ship-product"),
        quantity: numberValue("ship-qty")
    });
});

document.getElementById("shipment-list").addEventListener("click", (event) => {
    const button = event.target.closest("button");
    if (!button) return;
    const id = Number(button.dataset.id);
    if (button.dataset.action === "advance") api("/api/shipment/advance", { id });
    if (button.dataset.action === "delay") api("/api/shipment/delay", { id });
    if (button.dataset.action === "resume") api("/api/shipment/resume", { id });
});

document.getElementById("supplier-form").addEventListener("submit", (event) => {
    event.preventDefault();
    api("/api/supplier/add", {
        id: numberValue("supplier-id"),
        name: document.getElementById("supplier-name").value.trim(),
        contact: document.getElementById("supplier-contact").value.trim(),
        rating: numberValue("supplier-rating")
    });
});

document.getElementById("supplier-list").addEventListener("click", (event) => {
    const button = event.target.closest("button");
    if (!button || button.dataset.action !== "delete-supplier") return;
    const id = Number(button.dataset.id);
    if (confirm("Delete supplier " + id + "?")) api("/api/supplier/delete", { id });
});

document.getElementById("generate-report").addEventListener("click", async () => {
    const data = await api("/api/report", {});
    if (data && data.message) document.getElementById("report-output").textContent = data.message;
});

api("/api/state").then(() => showBanner(true, ""));
