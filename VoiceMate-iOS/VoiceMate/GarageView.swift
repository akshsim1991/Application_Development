import SwiftUI

struct GarageView: View {
    @ObservedObject private var g = Garage.shared

    @State private var editingFuel: FuelEntry?
    @State private var editingService: ServiceItem?
    @State private var editingDoc: DocItem?
    @State private var showOdo = false
    @State private var odoText = ""

    private func money(_ v: Double) -> String { v.formatted(.currency(code: Prefs.currencyCode)) }
    private func one(_ v: Double) -> String { String(format: "%.1f", v) }

    var body: some View {
        NavigationStack {
            List {
                odometerSection
                fuelSection
                serviceSection
                documentSection
            }
            .scrollContentBackground(.hidden)
            .background(Theme.bg.ignoresSafeArea())
            .keyboardDone()
            .navigationTitle("Garage")
            .alert("Odometer (km)", isPresented: $showOdo) {
                TextField("km", text: $odoText).keyboardType(.decimalPad)
                Button("Save") { if let v = Double(odoText) { g.setOdometer(v) } }
                Button("Cancel", role: .cancel) {}
            } message: {
                Text("Enter your bike's real reading.")
            }
            .sheet(item: $editingFuel) { item in
                FuelEditor(entry: item, isNew: !g.fuel.contains { $0.id == item.id },
                           onSave: { g.upsertFuel($0) }, onDelete: { g.deleteFuel(ids: [item.id]) })
            }
            .sheet(item: $editingService) { item in
                ServiceEditor(item: item, isNew: !g.services.contains { $0.id == item.id }, odometer: g.odometerKm,
                              onSave: { g.upsertService($0) }, onDelete: { g.deleteService(id: item.id) })
            }
            .sheet(item: $editingDoc) { item in
                DocEditor(item: item, isNew: !g.docs.contains { $0.id == item.id },
                          onSave: { g.upsertDoc($0) }, onDelete: { g.deleteDoc(id: item.id) })
            }
        }
    }

    // MARK: Sections

    private var odometerSection: some View {
        Section("Odometer") {
            Button {
                odoText = String(Int(g.odometerKm))
                showOdo = true
            } label: {
                HStack {
                    Text("Odometer")
                    Spacer()
                    Text("\(Int(g.odometerKm)) km").foregroundStyle(Theme.dim)
                    Image(systemName: "pencil").foregroundStyle(Theme.accent)
                }
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            Text("Tap to set your bike's real reading once. It then grows automatically with every ride. Voice: “set odometer to 12000”.")
                .font(.caption).foregroundStyle(Theme.dim)
        }
    }

    private var fuelSection: some View {
        Section("Fuel & mileage") {
            LabeledContent("Average mileage", value: g.averageMileage.map { "\(one($0)) km/l" } ?? "—")
            LabeledContent("Last fill-up", value: g.lastMileage.map { "\(one($0)) km/l" } ?? "—")
            LabeledContent("Cost per km", value: g.costPerKm.map { money($0) } ?? "—")
            LabeledContent("Total spent", value: money(g.totalSpent))
            Button {
                editingFuel = FuelEntry(date: Date(), litres: 0, pricePerLitre: 0, odometer: g.odometerKm)
            } label: { Label("Add fill-up", systemImage: "fuelpump.fill") }
            ForEach(g.fuelSorted.reversed()) { f in
                Button { editingFuel = f } label: {
                    VStack(alignment: .leading) {
                        Text("\(one(f.litres)) L · \(money(f.total))").font(.headline)
                        Text("\(f.date.formatted(date: .abbreviated, time: .omitted)) · at \(Int(f.odometer)) km")
                            .font(.caption).foregroundStyle(Theme.dim)
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .swipeActions { Button(role: .destructive) { g.deleteFuel(ids: [f.id]) } label: { Label("Delete", systemImage: "trash") } }
            }
            Text("Tap a fill-up to edit it. Fill the tank each time for correct mileage. Voice: “log fuel 8 litres at 107”.")
                .font(.caption).foregroundStyle(Theme.dim)
        }
    }

    private var serviceSection: some View {
        Section("Service reminders") {
            ForEach(g.services) { s in
                let left = s.remainingKm(odometer: g.odometerKm)
                Button { editingService = s } label: {
                    HStack {
                        VStack(alignment: .leading) {
                            Text(s.name).font(.headline)
                            Text("every \(Int(s.intervalKm)) km · last at \(Int(s.lastDoneKm)) km")
                                .font(.caption).foregroundStyle(Theme.dim)
                        }
                        Spacer()
                        Text(left <= 0 ? "Overdue \(Int(-left)) km" : "\(Int(left)) km left")
                            .font(.subheadline.weight(.semibold))
                            .foregroundStyle(left <= 0 ? Theme.bad : (left <= 200 ? Theme.accent : Theme.good))
                    }
                    .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .swipeActions(edge: .leading) {
                    Button { g.markDone(s.id) } label: { Label("Done", systemImage: "checkmark") }.tint(Theme.good)
                }
                .swipeActions(edge: .trailing) {
                    Button(role: .destructive) { g.deleteService(id: s.id) } label: { Label("Delete", systemImage: "trash") }
                }
            }
            Button {
                editingService = ServiceItem(name: "", intervalKm: 3000, lastDoneKm: g.odometerKm)
            } label: { Label("Add reminder", systemImage: "plus.circle.fill") }
            Text("Tap a reminder to edit it. Swipe right when you've done it. I announce anything due when you start a ride.")
                .font(.caption).foregroundStyle(Theme.dim)
        }
    }

    private var documentSection: some View {
        Section("Documents") {
            ForEach(g.docs) { d in
                Button { editingDoc = d } label: {
                    HStack {
                        VStack(alignment: .leading) {
                            Text(d.name).font(.headline)
                            Text(d.expiry.formatted(date: .abbreviated, time: .omitted)).font(.caption).foregroundStyle(Theme.dim)
                        }
                        Spacer()
                        Text(d.daysLeft < 0 ? "Expired" : "\(d.daysLeft) days")
                            .font(.subheadline.weight(.semibold))
                            .foregroundStyle(d.daysLeft < 0 ? Theme.bad : (d.daysLeft <= 30 ? Theme.accent : Theme.good))
                    }
                    .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .swipeActions { Button(role: .destructive) { g.deleteDoc(id: d.id) } label: { Label("Delete", systemImage: "trash") } }
            }
            Button {
                editingDoc = DocItem(name: "", expiry: Calendar.current.date(byAdding: .year, value: 1, to: Date()) ?? Date())
            } label: { Label("Add document", systemImage: "plus.circle.fill") }
            Text("Tap a document to edit it. Insurance, PUC, licence: I warn you 30 days before they expire.")
                .font(.caption).foregroundStyle(Theme.dim)
        }
    }
}

// MARK: - Editors

struct ServiceEditor: View {
    @Environment(\.dismiss) private var dismiss
    let original: ServiceItem
    let isNew: Bool
    let odometer: Double
    let onSave: (ServiceItem) -> Void
    let onDelete: () -> Void
    @State private var name: String
    @State private var interval: String
    @State private var lastDone: String

    init(item: ServiceItem, isNew: Bool, odometer: Double, onSave: @escaping (ServiceItem) -> Void, onDelete: @escaping () -> Void) {
        original = item
        self.isNew = isNew
        self.odometer = odometer
        self.onSave = onSave
        self.onDelete = onDelete
        _name = State(initialValue: item.name)
        _interval = State(initialValue: String(Int(item.intervalKm)))
        _lastDone = State(initialValue: String(Int(item.lastDoneKm)))
    }

    private var valid: Bool {
        !name.trimmingCharacters(in: .whitespaces).isEmpty && (Double(interval) ?? 0) > 0 && Double(lastDone) != nil
    }

    private var dueText: String {
        guard let i = Double(interval), let l = Double(lastDone) else { return "" }
        let left = l + i - odometer
        return left <= 0 ? "Overdue by \(Int(-left)) km" : "Due in \(Int(left)) km (at \(Int(l + i)) km)"
    }

    var body: some View {
        NavigationStack {
            Form {
                Section("Reminder") {
                    TextField("Name (e.g. Air filter)", text: $name)
                    HStack {
                        Text("Every (km)")
                        Spacer()
                        TextField("3000", text: $interval).keyboardType(.numberPad).multilineTextAlignment(.trailing).frame(width: 110)
                    }
                    HStack {
                        Text("Last done at (km)")
                        Spacer()
                        TextField("0", text: $lastDone).keyboardType(.numberPad).multilineTextAlignment(.trailing).frame(width: 110)
                    }
                    Button("Set last done to now (\(Int(odometer)) km)") { lastDone = String(Int(odometer)) }
                    if !dueText.isEmpty { Text(dueText).font(.footnote).foregroundStyle(Theme.accent) }
                }
                if !isNew {
                    Section {
                        Button("Delete reminder", role: .destructive) { onDelete(); dismiss() }
                    }
                }
            }
            .keyboardDone()
            .navigationTitle(isNew ? "New reminder" : "Edit reminder")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) { Button("Cancel") { dismiss() } }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Save") {
                        var item = original
                        item.name = name.trimmingCharacters(in: .whitespaces)
                        item.intervalKm = Double(interval) ?? original.intervalKm
                        item.lastDoneKm = Double(lastDone) ?? original.lastDoneKm
                        onSave(item)
                        dismiss()
                    }
                    .disabled(!valid)
                }
            }
        }
        .presentationDetents([.medium, .large])
        .preferredColorScheme(.dark)
    }
}

struct FuelEditor: View {
    @Environment(\.dismiss) private var dismiss
    let original: FuelEntry
    let isNew: Bool
    let onSave: (FuelEntry) -> Void
    let onDelete: () -> Void
    @State private var litres: String
    @State private var price: String
    @State private var odo: String
    @State private var date: Date

    init(entry: FuelEntry, isNew: Bool, onSave: @escaping (FuelEntry) -> Void, onDelete: @escaping () -> Void) {
        original = entry
        self.isNew = isNew
        self.onSave = onSave
        self.onDelete = onDelete
        _litres = State(initialValue: entry.litres > 0 ? String(entry.litres) : "")
        _price = State(initialValue: entry.pricePerLitre > 0 ? String(entry.pricePerLitre) : "")
        _odo = State(initialValue: String(Int(entry.odometer)))
        _date = State(initialValue: entry.date)
    }

    private var total: Double { (Double(litres) ?? 0) * (Double(price) ?? 0) }

    var body: some View {
        NavigationStack {
            Form {
                Section("Fill-up") {
                    HStack {
                        Text("Litres")
                        Spacer()
                        TextField("8", text: $litres).keyboardType(.decimalPad).multilineTextAlignment(.trailing).frame(width: 110)
                    }
                    HStack {
                        Text("Price per litre")
                        Spacer()
                        TextField("107", text: $price).keyboardType(.decimalPad).multilineTextAlignment(.trailing).frame(width: 110)
                    }
                    HStack {
                        Text("Odometer (km)")
                        Spacer()
                        TextField("0", text: $odo).keyboardType(.numberPad).multilineTextAlignment(.trailing).frame(width: 110)
                    }
                    DatePicker("Date", selection: $date, displayedComponents: .date)
                    if total > 0 { Text("Total \(total.formatted(.currency(code: Prefs.currencyCode)))").font(.footnote).foregroundStyle(Theme.accent) }
                }
                if !isNew {
                    Section { Button("Delete fill-up", role: .destructive) { onDelete(); dismiss() } }
                }
            }
            .keyboardDone()
            .navigationTitle(isNew ? "New fill-up" : "Edit fill-up")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) { Button("Cancel") { dismiss() } }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Save") {
                        var e = original
                        e.litres = Double(litres) ?? 0
                        e.pricePerLitre = Double(price) ?? 0
                        e.odometer = Double(odo) ?? original.odometer
                        e.date = date
                        onSave(e)
                        dismiss()
                    }
                    .disabled((Double(litres) ?? 0) <= 0)
                }
            }
        }
        .presentationDetents([.medium, .large])
        .preferredColorScheme(.dark)
    }
}

struct DocEditor: View {
    @Environment(\.dismiss) private var dismiss
    let original: DocItem
    let isNew: Bool
    let onSave: (DocItem) -> Void
    let onDelete: () -> Void
    @State private var name: String
    @State private var expiry: Date

    init(item: DocItem, isNew: Bool, onSave: @escaping (DocItem) -> Void, onDelete: @escaping () -> Void) {
        original = item
        self.isNew = isNew
        self.onSave = onSave
        self.onDelete = onDelete
        _name = State(initialValue: item.name)
        _expiry = State(initialValue: item.expiry)
    }

    var body: some View {
        NavigationStack {
            Form {
                Section("Document") {
                    TextField("Name (e.g. Insurance)", text: $name)
                    DatePicker("Expires on", selection: $expiry, displayedComponents: .date)
                }
                if !isNew {
                    Section { Button("Delete document", role: .destructive) { onDelete(); dismiss() } }
                }
            }
            .keyboardDone()
            .navigationTitle(isNew ? "New document" : "Edit document")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) { Button("Cancel") { dismiss() } }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Save") {
                        var d = original
                        d.name = name.trimmingCharacters(in: .whitespaces)
                        d.expiry = expiry
                        onSave(d)
                        dismiss()
                    }
                    .disabled(name.trimmingCharacters(in: .whitespaces).isEmpty)
                }
            }
        }
        .presentationDetents([.medium])
        .preferredColorScheme(.dark)
    }
}
