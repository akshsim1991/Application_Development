import SwiftUI

struct GarageView: View {
    @ObservedObject private var g = Garage.shared

    @State private var showFuel = false
    @State private var litres = ""
    @State private var price = ""
    @State private var showService = false
    @State private var svcName = ""
    @State private var svcInterval = ""
    @State private var showDoc = false
    @State private var docName = ""
    @State private var docDate = Calendar.current.date(byAdding: .year, value: 1, to: Date()) ?? Date()

    private func money(_ v: Double) -> String { v.formatted(.currency(code: Prefs.currencyCode)) }
    private func one(_ v: Double) -> String { String(format: "%.1f", v) }

    var body: some View {
        NavigationStack {
            List {
                Section("Odometer") {
                    HStack {
                        Text("Odometer (km)")
                        Spacer()
                        TextField("km", value: $g.odometerKm, format: .number.precision(.fractionLength(0)))
                            .keyboardType(.decimalPad).multilineTextAlignment(.trailing).frame(width: 120)
                    }
                    Text("Set it to your bike's real reading once. It then grows automatically with every ride. Voice: “set odometer to 12000”.")
                        .font(.caption).foregroundStyle(Theme.dim)
                }

                Section("Fuel & mileage") {
                    LabeledContent("Average mileage", value: g.averageMileage.map { "\(one($0)) km/l" } ?? "—")
                    LabeledContent("Last fill-up", value: g.lastMileage.map { "\(one($0)) km/l" } ?? "—")
                    LabeledContent("Cost per km", value: g.costPerKm.map { money($0) } ?? "—")
                    LabeledContent("Total spent", value: money(g.totalSpent))
                    Button { showFuel = true } label: { Label("Add fill-up", systemImage: "fuelpump.fill") }
                    ForEach(g.fuelSorted.reversed()) { f in
                        VStack(alignment: .leading) {
                            Text("\(one(f.litres)) L · \(money(f.total))").font(.headline)
                            Text("\(f.date.formatted(date: .abbreviated, time: .omitted)) · at \(Int(f.odometer)) km")
                                .font(.caption).foregroundStyle(Theme.dim)
                        }
                        .swipeActions { Button(role: .destructive) { g.deleteFuel(ids: [f.id]) } label: { Label("Delete", systemImage: "trash") } }
                    }
                    Text("Fill the tank each time for correct mileage. Voice: “log fuel 8 litres at 107”.")
                        .font(.caption).foregroundStyle(Theme.dim)
                }

                Section("Service reminders") {
                    ForEach(g.services) { s in
                        let left = s.remainingKm(odometer: g.odometerKm)
                        HStack {
                            VStack(alignment: .leading) {
                                Text(s.name).font(.headline)
                                Text("every \(Int(s.intervalKm)) km").font(.caption).foregroundStyle(Theme.dim)
                            }
                            Spacer()
                            Text(left <= 0 ? "Overdue \(Int(-left)) km" : "\(Int(left)) km left")
                                .font(.subheadline.weight(.semibold))
                                .foregroundStyle(left <= 0 ? Theme.bad : (left <= 200 ? Theme.accent : Theme.good))
                        }
                        .swipeActions(edge: .leading) {
                            Button { g.markDone(s.id) } label: { Label("Done", systemImage: "checkmark") }.tint(Theme.good)
                        }
                    }
                    .onDelete { g.deleteService(at: $0) }
                    Button { showService = true } label: { Label("Add reminder", systemImage: "plus.circle.fill") }
                    Text("Swipe right on a reminder when you've done it. I announce anything due when you start a ride.")
                        .font(.caption).foregroundStyle(Theme.dim)
                }

                Section("Documents") {
                    ForEach(g.docs) { d in
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
                    }
                    .onDelete { g.deleteDoc(at: $0) }
                    Button { showDoc = true } label: { Label("Add document", systemImage: "plus.circle.fill") }
                    Text("Insurance, PUC, licence: I warn you 30 days before they expire.")
                        .font(.caption).foregroundStyle(Theme.dim)
                }
            }
            .scrollContentBackground(.hidden)
            .background(Theme.bg.ignoresSafeArea())
            .navigationTitle("Garage")
            .alert("Add fill-up", isPresented: $showFuel) {
                TextField("Litres", text: $litres).keyboardType(.decimalPad)
                TextField("Price per litre", text: $price).keyboardType(.decimalPad)
                Button("Save") {
                    if let l = Double(litres) { g.logFuel(litres: l, pricePerLitre: Double(price) ?? 0) }
                    litres = ""; price = ""
                }
                Button("Cancel", role: .cancel) { litres = ""; price = "" }
            }
            .alert("Add service reminder", isPresented: $showService) {
                TextField("Name (e.g. Air filter)", text: $svcName)
                TextField("Every how many km", text: $svcInterval).keyboardType(.numberPad)
                Button("Save") {
                    if !svcName.isEmpty, let km = Double(svcInterval), km > 0 { g.addService(name: svcName, intervalKm: km) }
                    svcName = ""; svcInterval = ""
                }
                Button("Cancel", role: .cancel) { svcName = ""; svcInterval = "" }
            }
            .sheet(isPresented: $showDoc) {
                NavigationStack {
                    Form {
                        TextField("Name (e.g. Insurance)", text: $docName)
                        DatePicker("Expires on", selection: $docDate, displayedComponents: .date)
                    }
                    .navigationTitle("Add document")
                    .navigationBarTitleDisplayMode(.inline)
                    .toolbar {
                        ToolbarItem(placement: .cancellationAction) { Button("Cancel") { showDoc = false; docName = "" } }
                        ToolbarItem(placement: .confirmationAction) {
                            Button("Save") {
                                if !docName.isEmpty { g.addDoc(name: docName, expiry: docDate) }
                                docName = ""; showDoc = false
                            }
                        }
                    }
                }
                .presentationDetents([.medium])
                .preferredColorScheme(.dark)
            }
        }
    }
}
