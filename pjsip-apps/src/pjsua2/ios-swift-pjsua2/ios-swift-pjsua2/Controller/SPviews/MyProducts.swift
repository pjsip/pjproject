import SwiftUI

struct Product: Identifiable {
    let id = UUID()
    let name: String
    let price: Double
}

struct MyProductsView: View {
    @State private var products: [Product] = [
        Product(name: "Product 1", price: 19.99),
        Product(name: "Product 2", price: 29.99),
        Product(name: "Product 3", price: 39.99)
    ]
    @State private var showingAddProductSheet = false
    @State private var newProductName = ""
    @State private var newProductPrice = ""
    
    var body: some View {
        NavigationView {
            List {
                ForEach(products) { product in
                    VStack(alignment: .leading) {
                        Text(product.name)
                            .font(.headline)
                        Text("$\(product.price, specifier: "%.2f")")
                            .font(.subheadline)
                            .foregroundColor(.secondary)
                    }
                }
            }
            .navigationTitle("My Products")
            .navigationBarItems(trailing: Button(action: {
                showingAddProductSheet = true
            }) {
                Image(systemName: "plus")
            })
            .sheet(isPresented: $showingAddProductSheet) {
                AddProductView(products: $products, showingSheet: $showingAddProductSheet)
            }
        }
    }
}

struct AddProductView: View {
    @Binding var products: [Product]
    @Binding var showingSheet: Bool
    @State private var productName = ""
    @State private var productPrice = ""
    
    var body: some View {
        NavigationView {
            Form {
                TextField("Product Name", text: $productName)
                TextField("Price", text: $productPrice)
                    .keyboardType(.decimalPad)
            }
            .navigationTitle("Add New Product")
            .navigationBarItems(
                leading: Button("Cancel") {
                    showingSheet = false
                },
                trailing: Button("Add") {
                    if let price = Double(productPrice), !productName.isEmpty {
                        let newProduct = Product(name: productName, price: price)
                        products.append(newProduct)
                        showingSheet = false
                    }
                }
            )
        }
    }
}

struct MyProductsView_Previews: PreviewProvider {
    static var previews: some View {
        MyProductsView()
    }
}
