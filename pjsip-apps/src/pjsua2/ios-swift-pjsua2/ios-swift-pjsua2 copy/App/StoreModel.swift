//
//  StoreItems.swift
//  arnacon
//
//  Created by David Fassy on 29/02/2024.
//

import Foundation

struct StoreModel: Decodable {
    
    var items: [StoreItem]
    
    init() {
        self.items = []
    }
    
    init(from decoder: Decoder) throws {
        let container = try decoder.singleValueContainer()
        let dict = try container.decode([String: StoreItemDetail].self)
        self.items = dict.map { key, value in
            StoreItem(id: key, detail: value)
        }
    }
}

struct StoreItem: Identifiable {
    
    let id: String
    let detail: StoreItemDetail
}

struct StoreItemDetail: Decodable {
    
    let description: String
    let image: URL
    let name: String
    private let attributes: [StoreItemAttribute]
    
    var oneTimePrice: Double? {
        
        if let result = stringValue(forTraitType: "InitP") {
            return Double(result)
        }
        return nil
    }
    
    var subscriptionPrice: Double? {
        
        if let result = stringValue(forTraitType: "Price") {
            return Double(result)
        }
        return nil
    }
    
    var subscriptionPeriod: Int? {
        
        return integerValue(forTraitType: "Duration")
    }
    
    var currency: String? {
        
        return stringValue(forTraitType: "Currency")
    }
    
    func getDisplayOneTimePrice() -> String? {
        
        if let price = oneTimePrice, let currency = currency {
            return String(format: "%.2f %@", price, currency)
        }
        return nil
    }
    
    func getDisplaySubscriptionPrice() -> String? {
        
        if let price = subscriptionPrice, let currency = currency {
            return String(format: "%.2f %@", price, currency)
        }
        return nil
    }
    
    private func stringValue(forTraitType traitType: String) -> String? {
            
        for attribute in attributes {
            if attribute.trait_type == traitType {
                if case .string(let stringValue) = attribute.value {
                    return stringValue
                }
            }
        }
        return nil
    }
    
    private func integerValue(forTraitType traitType: String) -> Int? {
        for attribute in attributes {
            if attribute.trait_type == traitType {
                if case .integer(let integerValue) = attribute.value {
                    return integerValue
                }
            }
        }
        return nil
    }
    
    private func doubleValue(forTraitType traitType: String) -> Double? {
        for attribute in attributes {
            if attribute.trait_type == traitType {
                if case .double(let doubleValue) = attribute.value {
                    return doubleValue
                }
            }
        }
        return nil
    }
}

enum StoreItemAttributeValue: Decodable {
    
    case string(String)
    case integer(Int)
    case double(Double)
    
    init(from decoder: Decoder) throws {
        let container = try decoder.singleValueContainer()
        if let stringValue = try? container.decode(String.self) {
            self = .string(stringValue)
        } else if let integerValue = try? container.decode(Int.self) {
            self = .integer(integerValue)
        } else if let doubleValue = try? container.decode(Double.self) {
            self = .double(doubleValue)
        } else {
            throw DecodingError.typeMismatch(StoreItemAttributeValue.self, DecodingError.Context(codingPath: decoder.codingPath, debugDescription: "Expected a String, Integer or Double"))
        }
    }
}

struct StoreItemAttribute: Decodable {
    let trait_type: String
    let value: StoreItemAttributeValue
    let display_type: String?
    
    enum CodingKeys: String, CodingKey {
        case trait_type
        case value
        case display_type
    }
}
